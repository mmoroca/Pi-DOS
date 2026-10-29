#!/usr/bin/env python3
"""Exercise production USB optical discovery, MMC reads and digital audio without hardware."""
from pathlib import Path
import json
import shutil
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]
runtime = next((ROOT / 'output/buildroot-arm64/build').glob('pi-dos-runtime-*'))

with tempfile.TemporaryDirectory(prefix='pi-dos-cdrom.') as temporary:
    work = Path(temporary)
    block = work / 'sys/class/block'
    block.mkdir(parents=True)
    interface = work / 'sys/devices/usb/1-1/1-1:1.0'
    scsi = interface / 'host0/target0:0:0/0:0:0:0'
    scsi.mkdir(parents=True)
    (interface / 'bInterfaceClass').write_text('08\n')
    (scsi / 'type').write_text('5\n')
    (scsi / 'model').write_text('USB DVD/BD\n')
    optical = block / 'sr0'
    optical.mkdir()
    (optical / 'device').symlink_to(scsi)
    (optical / 'diskseq').write_text('10\n')
    stick = block / 'sda'
    stick.mkdir()
    (stick / 'device').mkdir()
    (stick / 'device/type').write_text('0\n')
    sata = block / 'sr1'
    sata.mkdir()
    (sata / 'device').mkdir()
    (sata / 'device/type').write_text('5\n')

    # Use the pinned runtime's real abstract CD interface, including our patch,
    # instead of duplicating its ABI in the fixture.
    header = (runtime / 'src/dos/cdrom.h').read_text()
    if 'ID_USB' not in header:
        (work / 'src/dos').mkdir(parents=True)
        for name in ('cdrom.h', 'dos_mscdex.cpp', 'dos_programs.cpp', 'drives.h', 'drive_iso.cpp'):
            shutil.copyfile(runtime / 'src/dos' / name, work / 'src/dos' / name)
        subprocess.run(['patch', '-d', str(work), '-p1', '--batch', '--fuzz=0', '-i',
                        str(ROOT / 'patches/dosbox-x/0019-automatic-usb-cdrom.patch')],
                       check=True, capture_output=True)
        header = (work / 'src/dos/cdrom.h').read_text()
    structs = header[header.index('typedef struct SMSF'):header.index('template<typename T1')]
    base = header[header.index('class CDROM_Interface\n'):header.index('//! \\brief CD-ROM interface to SDL')]
    (work / 'test-cdrom-abi.h').write_text(structs + base)
    iso_path = work / 'src/dos/drive_iso.cpp' if (work / 'src/dos/drive_iso.cpp').exists() else runtime / 'src/dos/drive_iso.cpp'
    iso = iso_path.read_text()
    start = iso.index('bool isoFile::Read(')
    read = iso[start:iso.index('\n#endif', start)]
    start = iso.index('inline bool isoDrive :: readSector(')
    sector = iso[start:iso.index('\n}', start) + 2]
    media = iso[iso.index('// Native optical media are replaceable'):]
    (work / 'test-cdrom-media.h').write_text(sector + '\n' + read + '\n' + media)
    shutil.copyfile(ROOT / 'src/runtime/dos_sharing.h', work / 'pidos_dos_sharing.h')
    for name in ('linux', 'player', 'backend'):
        content = (ROOT / f'src/runtime/usb_cdrom_{name}.h').read_text()
        if name == 'player':
            content = content.replace('"/sys/class/block"', json.dumps(str(block)))
            content = content.replace('"/dev"', json.dumps(str(work / 'dev')))
        (work / f'pidos_usb_cdrom_{name}.h').write_text(content)
    executable = work / 'check'
    subprocess.run(['g++', '-std=c++14', '-O1', '-Wall', '-Wextra', '-Werror', '-pthread',
                    '-I', str(work), str(ROOT / 'tools/image/usb-cdrom-test.cpp'), '-o', str(executable)], check=True)
    subprocess.run([str(executable), str(work)], check=True, timeout=30)
print('USB optical: discovery, disc changes, stale DOS handles, MMC/raw reads, CD-DA mixer and playback controls passed')
