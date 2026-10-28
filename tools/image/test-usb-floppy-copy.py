#!/usr/bin/env python3
"""Check the actual patched COPY selection and loop with in-memory DOS handles."""
from pathlib import Path
import argparse
import subprocess
import tempfile

root = Path(__file__).resolve().parents[2]
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('runtime', type=Path, nargs='?')
args = parser.parse_args()
runtime = args.runtime or next((root / 'output/buildroot-arm64/build').glob('pi-dos-runtime-*'), None)
if runtime is None:
    parser.error('a runtime source directory is required')
source = (runtime / 'src/shell/shell_cmds.cpp').read_text()

with tempfile.TemporaryDirectory(prefix='pi-dos-floppy-copy.') as temporary:
    work = Path(temporary)
    if 'static uint16_t pidos_copy_chunk(' not in source:
        # Exercise the proposed patch before a full image rebuild as well.
        shell = work / 'src/shell/shell_cmds.cpp'
        shell.parent.mkdir(parents=True)
        shell.write_text(source)
        subprocess.run(['patch', '-d', str(work), '-p1', '--batch', '--fuzz=0', '-i',
                        str(root / 'patches/dosbox-x/0017-usb-floppy-copy-buffer.patch')],
                       check=True, capture_output=True, text=True)
        source = shell.read_text()
    start = source.index('static uint16_t pidos_copy_chunk(')
    helper = source[start:source.index('\n}', start) + 2]
    start = source.index('// Copy\n', source.index('void DOS_Shell::CMD_COPY('))
    loop = source[start:source.index('if (!DOS_CloseFile(sourceHandle)) failed=true;', start)]
    prefix = r'''
#include <algorithm>
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>
const unsigned int DOS_FILES=16, DOS_DRIVES=26;
struct DOS_File {
    uint8_t drive=0;
    uint16_t info=0;
    uint8_t GetDrive(){return drive;}
    uint16_t GetInformation(){return info;}
};
struct DOS_Drive {bool native=false;};
static DOS_File *Files[DOS_FILES]={};
static DOS_Drive *Drives[DOS_DRIVES]={};
static uint8_t handles[64];
static uint8_t RealHandle(uint16_t h){return h<64?handles[h]:255;}
static bool PI_DOS_IsUSBFloppy(DOS_Drive *drive){return drive->native;}
static std::vector<uint8_t> input,output;
static std::vector<uint16_t> requests,writes;
static size_t position=0;
static bool read_failure=false,write_failure=false;
static bool DOS_ReadFile(uint16_t h,uint8_t *buffer,uint16_t *amount){
    assert(h==40);requests.push_back(*amount);
    if(read_failure){*amount=0;return false;}
    *amount=std::min<size_t>(*amount,input.size()-position);
    std::copy_n(input.begin()+position,*amount,buffer);position+=*amount;
    return true;
}
static bool DOS_WriteFile(uint16_t h,uint8_t *buffer,uint16_t *amount){
    assert(h==41);
    if(write_failure){*amount=0;return false;}
    writes.push_back(*amount);output.insert(output.end(),buffer,buffer+*amount);
    return true;
}
static unsigned int DOS_FindDevice(const char *name){return !strcmp(name,"con")?0:255;}
static bool DOS_CloseFile(uint16_t){return true;}
static bool DOS_UnlinkFile(const char*){return true;}
static struct {bool echo=false;unsigned int errorcode=0;void dta(uint32_t){}} dos;
struct DOS_Shell {
    bool echo=false,failed_result=false;
    void transfer(bool console=false);
};
static bool CheckBreak(DOS_Shell*){return false;}
'''
    body = r'''
void DOS_Shell::transfer(bool console){
    const uint16_t sourceHandle=40,targetHandle=41;
    const char *name=console?"con":"source",*nameTarget="target";
    const uint32_t save_dta=0;
    const bool exist=true;
''' + loop + r'''
    failed_result=failed;
}
static void reset(size_t size){
    input.resize(size);
    for(size_t i=0;i<size;++i) input[i]=static_cast<uint8_t>((i*17+i/251)%256);
    output.clear();requests.clear();writes.clear();position=0;
    read_failure=write_failure=false;
}
int main(){
    unsetenv("PI_DOS_DIAGNOSTICS");
    std::fill_n(handles,64,255);handles[40]=3;handles[41]=7;
    DOS_File source,target;source.drive=2;
    DOS_Drive floppy,other;floppy.native=true;
    Files[3]=&source;Files[7]=&target;Drives[0]=&floppy;Drives[2]=&other;
    assert(pidos_copy_chunk(40,41)==61440);
    source.info=0x80;assert(pidos_copy_chunk(40,41)==32768);source.info=0;
    target.info=0x80;assert(pidos_copy_chunk(40,41)==32768);target.info=0;
    target.drive=2;assert(pidos_copy_chunk(40,41)==32768);target.drive=0;
    target.drive=255;assert(pidos_copy_chunk(40,41)==32768);target.drive=0;
    Drives[0]=nullptr;assert(pidos_copy_chunk(40,41)==32768);Drives[0]=&floppy;
    Files[7]=nullptr;assert(pidos_copy_chunk(40,41)==32768);Files[7]=&target;
    assert(pidos_copy_chunk(65535,41)==32768&&pidos_copy_chunk(40,65535)==32768);
    // Both chunk sizes, partial and exact EOF boundaries, and complete payloads.
    for(bool native:{false,true}){
        floppy.native=native;
        const uint16_t chunk=native?61440:32768;
        for(size_t size:{0,1,32767,32768,32769,61439,61440,61441,122880,580391}){
            reset(size);DOS_Shell shell;shell.transfer();
            assert(!shell.failed_result&&input==output&&requests.front()==chunk);
            assert(*std::max_element(requests.begin(),requests.end())==chunk);
            assert(static_cast<size_t>(std::count_if(writes.begin(),writes.end(),
                [](uint16_t n){return n!=0;}))==(size+chunk-1)/chunk);
        }
    }
    floppy.native=true;
    // Concatenation appends, while device-source Ctrl-Z retains 32 KiB policy.
    reset(61441);output={1,2,3};DOS_Shell shell;shell.transfer();
    assert(!shell.failed_result&&output.size()==input.size()+3&&output[2]==3);
    assert(std::equal(input.begin(),input.end(),output.begin()+3));
    reset(100);input.assign(100,'X');input[50]=26;source.info=0x80;
    shell.transfer(true);assert(output.size()==50&&requests.front()==32768);source.info=0;
    reset(61441);read_failure=true;shell.transfer();assert(shell.failed_result&&output.empty());
    reset(61441);write_failure=true;shell.transfer();assert(shell.failed_result&&output.empty());
    setenv("PI_DOS_DIAGNOSTICS","1",1);assert(pidos_copy_chunk(40,41)==61440);
    puts("PASS: actual COPY loop: USB target handles, regular/device policies, 32/60 KiB boundaries, full payloads, EOF, concatenation and errors");
}
'''
    check = work / 'check.cpp'
    check.write_text(prefix + helper + body)
    executable = work / 'check'
    subprocess.run(['g++', '-std=c++11', '-DLINUX', '-Wall', '-Wextra', '-Werror',
                    str(check), '-o', str(executable)], check=True)
    checked = subprocess.run([str(executable)], check=True, capture_output=True, text=True)
    assert checked.stderr.count('Pi-DOS USB floppy COPY: chunk_bytes=61440') == 1
    print(checked.stdout.strip())
