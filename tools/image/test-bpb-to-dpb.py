#!/usr/bin/env python3
"""Check the production INT 21h AH=53h handler without touching any disks."""
from pathlib import Path
import os
import subprocess
import tempfile

root = Path(__file__).resolve().parents[2]
runtime = next((root / 'output/buildroot-arm64/build').glob('pi-dos-runtime-*'))
source = (runtime / 'src/dos/dos.cpp').read_text()
with tempfile.TemporaryDirectory(prefix='pi-dos-dpb.') as temporary:
    work = Path(temporary)
    if 'E_Exit("Unhandled Dos 21 call %02X",reg_ah);' in source:
        target = work / 'src/dos/dos.cpp'
        target.parent.mkdir(parents=True)
        target.write_text(source)
        subprocess.run(['patch', '-d', str(work), '-p1', '--batch', '--fuzz=0', '-i',
                        str(root / 'patches/dosbox-x/0043-dos-bpb-to-dpb.patch')], check=True)
        source = target.read_text()
    start = source.index('        case 0x53:')
    handler = source[start:source.index('        case 0x54:', start)]
    (work / 'handler.h').write_text(handler)
    (work / 'check.cpp').write_text(r'''
#include <cassert>
#include <cstdint>
#include <cstring>
#include <cstdio>
using PhysPt = uint32_t;
static uint8_t memory[0x40000];
static uint16_t reg_ax, reg_si = 0x121, reg_bp = 0x233;
#define reg_ah (reg_ax >> 8)
static PhysPt ds = 0x10000, es = 0x20000;
#define SegPhys(segment) (segment)
static struct { struct { unsigned int major = 5; } version; } dos;
enum { DOSERR_DATA_INVALID = 13 };
static unsigned int error;
static bool carry;
static void DOS_SetError(unsigned int e) { error = e; }
static void CALLBACK_SCF(bool value) { carry = value; }
static uint8_t mem_readb(PhysPt p) { assert(p < sizeof(memory)); return memory[p]; }
static uint16_t mem_readw(PhysPt p) { return mem_readb(p) | (uint16_t(mem_readb(p+1)) << 8); }
static uint32_t mem_readd(PhysPt p) { return mem_readw(p) | (uint32_t(mem_readw(p+2)) << 16); }
static void mem_writeb(PhysPt p, uint8_t v) { assert(p < sizeof(memory)); memory[p] = v; }
static void mem_writew(PhysPt p, uint16_t v) { mem_writeb(p,v); mem_writeb(p+1,v>>8); }
static void mem_writed(PhysPt p, uint32_t v) { mem_writew(p,v); mem_writew(p+2,v>>16); }
static void call() { reg_ax = 0x5300; switch (reg_ah) {
#include "handler.h"
} }
static void bpb(uint8_t cluster, uint16_t roots, uint32_t total, uint16_t fat, uint8_t media) {
    memset(memory, 0xa5, sizeof(memory));
    const PhysPt p = ds + reg_si;
    mem_writew(p, 512); mem_writeb(p+2, cluster); mem_writew(p+3, 1);
    mem_writeb(p+5, 2); mem_writew(p+6, roots);
    mem_writew(p+8, total > 65535 ? 0 : total);
    mem_writeb(p+10, media); mem_writew(p+11, fat);
    mem_writed(p+17, 63); // Hidden sectors must not be added to DPB offsets.
    mem_writed(p+21, total);
}
static const uint8_t floppy1440[] = {
    0xa5,0xa5, 0,2, 0,0, 1,0, 2, 0xe0,0, 0x21,0, 0x20,0x0b,
    9,0, 0x13,0, 0xa5,0xa5,0xa5,0xa5, 0xf0,0xa5,
    0xa5,0xa5,0xa5,0xa5, 0,0, 0xff,0xff
};
static const uint8_t floppy720[] = {
    0xa5,0xa5, 0,2, 1,1, 1,0, 2, 0x70,0, 0x0e,0, 0xca,2,
    3,0, 7,0, 0xa5,0xa5,0xa5,0xa5, 0xf9,0xa5,
    0xa5,0xa5,0xa5,0xa5, 0,0, 0xff,0xff
};
static void expected(const uint8_t *bytes, size_t size) {
    const PhysPt p = es + reg_bp;
    assert(!carry && reg_ax == 0x5300);
    assert(ds == 0x10000 && es == 0x20000 && reg_si == 0x121 && reg_bp == 0x233);
    assert(!memcmp(memory+p, bytes, size));
    assert(memory[p-1] == 0xa5 && memory[p+size] == 0xa5);
}
static void rejected() {
    uint8_t before[64]; memcpy(before, memory+es+reg_bp, sizeof(before));
    carry = false; error = 0; call();
    assert(carry && reg_ax == DOSERR_DATA_INVALID && error == DOSERR_DATA_INVALID);
    assert(!memcmp(before, memory+es+reg_bp, sizeof(before)));
}
int main() {
    bpb(1, 224, 2880, 9, 0xf0); carry = true; call();
    expected(floppy1440, sizeof(floppy1440));
    bpb(2, 112, 1440, 3, 0xf9); call(); expected(floppy720, sizeof(floppy720));
    // DOS 3.x has a byte FAT size and all later fields move back one byte.
    dos.version.major = 3; bpb(1, 224, 2880, 9, 0xf0); call();
    uint8_t legacy[32]; memcpy(legacy, floppy1440, 16);
    memcpy(legacy+16, floppy1440+17, 16); expected(legacy, sizeof(legacy));
    // DOS 2.x initializes the current-directory cluster, preserving its path.
    dos.version.major = 2; bpb(1, 224, 2880, 9, 0xf0); call();
    legacy[30] = legacy[31] = 0xa5; expected(legacy, sizeof(legacy));
    dos.version.major = 5;
    // A 64 MiB FAT16 partition uses the 32-bit total-sector BPB field.
    bpb(4, 512, 131072, 128, 0xf8); call();
    assert(!carry && mem_readw(es+reg_bp+11) == 289);
    assert(mem_readw(es+reg_bp+13) == 32696);
    assert(mem_readw(es+reg_bp+15) == 128 && mem_readw(es+reg_bp+17) == 257);
    // Non-sector-aligned root sizes round up to a complete directory sector.
    bpb(1, 225, 2880, 9, 0xf0); call();
    assert(!carry && mem_readw(es+reg_bp+11) == 34);
    // Invalid values must neither divide by zero nor partially change a DPB.
    bpb(0, 224, 2880, 9, 0xf0); rejected();
    bpb(3, 224, 2880, 9, 0xf0); rejected();
    bpb(1, 224, 2880, 9, 0xf0); mem_writew(ds+reg_si, 0); rejected();
    bpb(1, 224, 2880, 9, 0xf0); mem_writew(ds+reg_si, 500); rejected();
    bpb(1, 224, 2880, 9, 0xf0); mem_writew(ds+reg_si+3, 0); rejected();
    bpb(1, 224, 2880, 9, 0xf0); mem_writeb(ds+reg_si+5, 0); rejected();
    bpb(1, 0, 131072, 0, 0xf8); rejected(); // Unsupported FAT32.
    bpb(1, 224, 32, 9, 0xf0); rejected(); // Metadata exceeds the volume.
    bpb(1, 224, 1000000, 9, 0xf8); rejected(); // DPB cluster count overflow.
    bpb(1, 224, 1000000, 40000, 0xf8); rejected(); // DPB sector overflow.
    dos.version.major = 3; bpb(1, 224, 10000, 256, 0xf8); rejected();
    // Returning from the same interrupt again proves invalid input is nonfatal.
    dos.version.major = 5; bpb(1, 224, 2880, 9, 0xf0); call();
    expected(floppy1440, sizeof(floppy1440));
    puts("PASS: actual AH=53h handler: FAT12/FAT16, DOS 2/3/4+ layouts, caller fields, invalid BPBs and recovery");
}
''')
    subprocess.run(['g++', '-std=c++14', '-O1', '-Wall', '-Wextra', '-Werror',
                    '-fsanitize=address,undefined', '-fno-omit-frame-pointer',
                    str(work / 'check.cpp'), '-o', str(work / 'check')], check=True)
    # LeakSanitizer cannot run inside the execution sandbox's ptrace wrapper.
    subprocess.run([str(work / 'check')], check=True,
                   env={**os.environ, 'ASAN_OPTIONS': 'detect_leaks=0'})
