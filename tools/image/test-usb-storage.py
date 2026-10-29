#!/usr/bin/env python3
"""Check production USB discovery, mounting policy and durable DOS file access.

Uses sparse FAT/exFAT images and simulated kernel mounts; never mounts a host disk.
"""
from pathlib import Path
import json
import shutil
import struct
import subprocess
import tempfile
import tarfile

ROOT = Path(__file__).resolve().parents[2]

with tempfile.TemporaryDirectory(prefix='pi-dos-usb-storage.') as temporary:
    work = Path(temporary)
    # AArch64 uses unsigned char. dosfstools 4.2's label validator interprets
    # OEM bytes as controls on signed-char hosts, so build the pinned tool with
    # the target's char semantics for this host-side filesystem test.
    with tarfile.open(ROOT / 'dl/buildroot/dosfstools/dosfstools-4.2.tar.gz') as archive:
        archive.extractall(work, filter='data')
    fattools = work / 'dosfstools-4.2'
    subprocess.run(['./configure', '--without-udev', 'CFLAGS=-O1 -funsigned-char'],
                   cwd=fattools, check=True, capture_output=True)
    subprocess.run(['make', '-C', 'src', '-j2', 'fatlabel'],
                   cwd=fattools, check=True, capture_output=True)
    (work / 'dev').mkdir()
    (work / 'sys/class/block').mkdir(parents=True)
    (work / 'sys/dev/block').mkdir(parents=True)
    (work / 'mountinfo').write_text('')
    (work / 'mounts').mkdir()
    (work / 'system-root').mkdir()
    numbers = {}

    def disk(name, number, kind='usb', subclass='06', model='External disk'):
        base = work / f'sys/devices/{kind}/{name}/interface'
        scsi = base / 'host/target/scsi'
        path = scsi / 'block' / name
        path.mkdir(parents=True)
        (base / 'bInterfaceClass').write_text('08' if kind == 'usb' else '00')
        (base / 'bInterfaceSubClass').write_text(subclass)
        (scsi / 'type').write_text('0')
        (scsi / 'model').write_text(model)
        (path / 'device').symlink_to(scsi)
        (path / 'dev').write_text(f'8:{number}')
        (path / 'diskseq').write_text('1')
        (path / 'size').write_text('524288')
        (path / 'removable').write_text('0')
        (work / 'sys/class/block' / name).symlink_to(path)
        (work / 'sys/dev/block' / f'8:{number}').symlink_to(path)
        with (work / 'dev' / name).open('wb') as out:
            out.truncate(256 * 1024 * 1024)
        numbers[name] = number
        return path

    def filesystem(name, fat, size):
        with (work / 'dev' / name).open('wb') as out:
            out.truncate(size * 1024 * 1024)
        command = ['mkfs.exfat'] if fat == 'exfat' else ['mkfs.fat', '-F', str(fat)]
        subprocess.run(command + [str(work / 'dev' / name)], check=True, capture_output=True)

    def partition(disk_path, name, index, number, fat, size, start):
        path = disk_path / name
        path.mkdir()
        (path / 'partition').write_text(str(index))
        (path / 'dev').write_text(f'8:{number}')
        (path / 'start').write_text(str(start))
        (path / 'size').write_text(str(size * 2048))
        (work / 'sys/class/block' / name).symlink_to(path)
        (work / 'sys/dev/block' / f'8:{number}').symlink_to(path)
        filesystem(name, fat, size)
        numbers[name] = number

    a = disk('sda', 0)
    subprocess.run(['sfdisk', str(work / 'dev/sda')], input=
                   'label: dos\nstart=2048,size=8192,type=1\nstart=10240,size=32768,type=6\nstart=43008,size=131072,type=c\n',
                   text=True, check=True, capture_output=True)
    partition(a, 'sda1', 1, 1, 12, 4, 2048)
    partition(a, 'sda2', 2, 2, 16, 16, 10240)
    partition(a, 'sda3', 3, 3, 32, 64, 43008)
    b = disk('sdb', 16)
    filesystem('sdb', 'exfat', 32)  # No partition table.
    (b / 'size').write_text(str(32 * 2048))

    c = disk('sdc', 32)
    # MBR extended container and its EBR: logical FAT16 volume at partition 5.
    def entry(kind, start, size):
        return bytes([0, 0, 0, 0, kind, 0, 0, 0]) + struct.pack('<II', start, size)
    with (work / 'dev/sdc').open('r+b') as out:
        mbr = bytearray(512)
        mbr[446:462] = entry(0x0f, 2048, 131072)
        mbr[510:512] = b'\x55\xaa'
        out.write(mbr)
        ebr = bytearray(512)
        ebr[446:462] = entry(6, 2048, 32768)
        ebr[510:512] = b'\x55\xaa'
        out.seek(2048 * 512)
        out.write(ebr)
    partition(c, 'sdc5', 5, 37, 16, 16, 4096)

    g = disk('sdg', 96)
    subprocess.run(['sfdisk', str(work / 'dev/sdg')], input=
                   'label: gpt\nstart=2048,size=32768,type=U\nstart=34816,size=32768,type=EBD0A0A2-B9E5-4433-87C0-68B6B72699C7\nstart=67584,size=32768,type=EBD0A0A2-B9E5-4433-87C0-68B6B72699C7,attrs="GUID:62"\n',
                   text=True, check=True, capture_output=True)
    partition(g, 'sdg1', 1, 97, 16, 16, 2048)  # EFI: skip.
    partition(g, 'sdg2', 2, 98, 'exfat', 16, 34816)
    partition(g, 'sdg3', 3, 99, 16, 16, 67584)  # Hidden: skip.
    system = disk('sdh', 112)
    filesystem('sdh', 16, 16)  # USB boot/system disk must never become E:.
    (system / 'size').write_text(str(16 * 2048))
    disk('sdi', 128, subclass='04', model='USB floppy')
    optical = disk('sr0', 160)
    (optical / 'device/type').write_text('5')
    disk('sdl', 176, kind='ata')
    (work / 'numbers').write_text(''.join(f'{name} {number}\n' for name, number in numbers.items()))

    for name in ('io', 'linux', 'dos'):
        content = (ROOT / f'src/runtime/usb_storage_{name}.h').read_text()
        for old, new in (('/sys/class/block', str(work / 'sys/class/block')),
                         ('/sys/class/block/', str(work / 'sys/class/block') + '/'),
                         ('/sys/dev/block/', str(work / 'sys/dev/block') + '/'),
                         ('/dev/', str(work / 'dev') + '/'),
                         ('/proc/self/mountinfo', str(work / 'mountinfo')),
                         ('/run/pi-dos-usb', str(work / 'mounts'))):
            content = content.replace(json.dumps(old), json.dumps(new))
        content = content.replace('std::chrono::steady_clock::now()', 'test_now()')
        content = content.replace('"/sbin/fatlabel"', json.dumps(str(fattools / 'src/fatlabel')))
        (work / f'pidos_usb_storage_{name}.h').write_text(content)
    shutil.copyfile(ROOT / 'src/runtime/usb_storage_api.h', work / 'pidos_usb_storage_api.h')
    shutil.copyfile(ROOT / 'src/runtime/drive_policy.h', work / 'pidos_drive_policy.h')
    system = (ROOT / 'src/runtime/system_drive_dos.h').read_text()
    system = system.replace('"/dosdata/"', json.dumps(str(work / 'system-root') + '/'))
    system = system.replace('"/dosdata"', json.dumps(str(work / 'system-root')))
    (work / 'pidos_system_drive_dos.h').write_text(system)
    runtime = next((ROOT / 'output/buildroot-arm64/build').glob('pi-dos-runtime-*'))
    dos_files = (runtime / 'src/dos/dos_files.cpp').read_text()
    start = dos_files.index('bool DOS_SetVolumeLabel(')
    (work / 'checked-label.h').write_text(dos_files[start:dos_files.index('\n}', start) + 2])
    executable = work / 'check'
    subprocess.run(['g++', '-std=c++14', '-O1', '-Wall', '-Wextra', '-Werror', '-pthread',
                    '-I', str(work), str(ROOT / 'tools/image/usb-storage-test.cpp'),
                    '-lblkid', '-o', str(executable)], check=True)
    result = subprocess.run([str(executable), str(work)], capture_output=True, text=True, timeout=30)
    if result.returncode:
        raise RuntimeError(result.stdout + result.stderr)
print('USB storage: discovery, file I/O, persistent FAT/exFAT labels, label errors, durable writes and display processing passed')
