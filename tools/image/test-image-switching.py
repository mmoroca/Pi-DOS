#!/usr/bin/env python3
"""Check image-set selection and media notifications without touching devices."""
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]
with tempfile.TemporaryDirectory(prefix='pidos-image-switch.') as temporary:
    work = Path(temporary)
    source = work / 'switch.cpp'
    source.write_text(r'''
#include <cassert>
struct DOS_Drive {
    int caches=0, changes=0;
    virtual ~DOS_Drive() = default;
    virtual void EmptyCache() { ++caches; }
    virtual void MediaChange() { ++changes; }
};
struct isoDrive : DOS_Drive {
    int immediate=0;
    void MediaChangeImmediate() { ++immediate; }
};
struct fatDrive : DOS_Drive {};
static DOS_Drive *Drives[26] = {};
static bool imageDiskChange[6] = {};
static bool dos_kernel_disabled=false;
struct DriveManager {
    static int sizes[26], positions[26];
    static DOS_Drive *next[26];
    static int GetDisksSize(int drive) { return sizes[drive]; }
    static void CycleDisks(int drive, bool notify) {
        assert(notify);
        positions[drive]=(positions[drive]+1)%sizes[drive];
        if(next[drive]) Drives[drive]=next[drive];
    }
};
int DriveManager::sizes[26]={}, DriveManager::positions[26]={};
DOS_Drive *DriveManager::next[26]={};
#include "image_switch.h"
int main() {
    isoDrive physical, single, cd1, cd2, protected_c, protected_z;
    fatDrive floppy1, floppy2, single_floppy;
    DOS_Drive usb;
    Drives[0]=&floppy1; DriveManager::sizes[0]=2;
    DriveManager::next[0]=&floppy2;
    Drives[1]=&single_floppy; DriveManager::sizes[1]=1;
    Drives[2]=&protected_c; DriveManager::sizes[2]=2;
    Drives[3]=&physical; DriveManager::sizes[3]=0;
    Drives[4]=&cd1; DriveManager::sizes[4]=2;
    DriveManager::next[4]=&cd2;
    Drives[5]=&single; DriveManager::sizes[5]=1;
    Drives[6]=&usb; DriveManager::sizes[6]=2;
    Drives[25]=&protected_z; DriveManager::sizes[25]=2;
    PiDosNextImage(true);
    assert(Drives[4]==&cd2 && DriveManager::positions[4]==1);
    assert(cd2.caches==1 && cd2.immediate==1 && cd2.changes==0);
    assert(cd1.caches==0 && floppy1.caches==0 && !imageDiskChange[0]);
    assert(physical.caches==0 && single.caches==0 && usb.caches==0);
    assert(protected_c.caches==0 && protected_z.caches==0);
    dos_kernel_disabled=true;
    PiDosNextImage(true);
    assert(DriveManager::positions[4]==0 && cd2.changes==1);
    PiDosNextImage(false);
    assert(Drives[0]==&floppy2 && DriveManager::positions[0]==1);
    assert(floppy2.caches==1 && floppy2.changes==1 && imageDiskChange[0]);
    assert(single_floppy.caches==0 && !imageDiskChange[1]);
    assert(cd2.caches==2); // floppy action leaves CDs alone
}
''')
    binary = work / 'switch'
    subprocess.run(['c++', '-std=c++14', '-Wall', '-Wextra', '-Werror',
                    '-I'+str(ROOT/'src/runtime'), str(source), '-o', str(binary)], check=True)
    subprocess.run([str(binary)], check=True)
print('PASS: CD/floppy image sets, wraparound, new-media cache/BIOS/IDE notifications; physical, single and protected drives unchanged.')
