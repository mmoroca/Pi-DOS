#!/usr/bin/env python3
"""Root-only integration test using disposable loop devices, never an SD card.

Arguments: compiled tests/boot/expand-data.c probe, 512 MiB PI-DOS FAT32 fixture
containing c/KEEP.TXT, and an original Pi-DOS 512-byte MBR.
"""
import fcntl
import os
from pathlib import Path
import struct
import subprocess
import sys
import tempfile

START = 526336 * 512
DISK_BYTES = 2 * 1024 * 1024 * 1024
SENTINEL = b'Pi-DOS BOOT preservation sentinel'
KEEP = b'Pi-DOS persistence during first-boot expansion\n'


def run(*args):
    return subprocess.check_output(args, stderr=subprocess.STDOUT, text=True)


def check(probe, filesystem, original, mode, root):
    disk_bytes = 64 * 1024**3 if mode == 'large-card' else DISK_BYTES
    mbr = bytearray(original)
    struct.pack_into('<I', mbr, 474, filesystem.stat().st_size // 512)
    if mode == 'resume':
        struct.pack_into('<I', mbr, 474, (disk_bytes - START) // 512)
    elif mode == 'extra-partition':
        struct.pack_into('<B', mbr, 482, 0x83)
        struct.pack_into('<II', mbr, 486, 1800 * 2048, 16 * 2048)
    elif mode == 'foreign-signature':
        struct.pack_into('<I', mbr, 440, 0x12345678)
    card = root / 'card.img'
    with card.open('wb') as image:
        image.truncate(disk_bytes)
        image.write(mbr)
        image.seek(1048576)
        image.write(SENTINEL)
        # Preserve holes: the large-card fixture consumes only written FAT/data.
        with filesystem.open('rb') as source:
            at = 0
            while at < filesystem.stat().st_size:
                try:
                    data = os.lseek(source.fileno(), at, os.SEEK_DATA)
                except OSError as error:
                    if error.errno == 6:  # ENXIO: the remainder is a hole.
                        break
                    raise
                end = os.lseek(source.fileno(), data, os.SEEK_HOLE)
                source.seek(data)
                image.seek(START + data)
                while data < end:
                    chunk = source.read(min(1024 * 1024, end - data))
                    image.write(chunk)
                    data += len(chunk)
                at = end
    loop = run('losetup', '--find', '--show', '--partscan', str(card)).strip()
    part = loop + 'p2'
    try:
        output = run(str(probe), part)
        print(output, end='')
        if mode in ('extra-partition', 'foreign-signature'):
            assert 'Expansion result: 0' in output
            with card.open('rb') as image:
                assert image.read(512) == mbr, 'Rejected layout was modified'
            print(f'PASS: {mode} left unchanged')
            return
        assert 'Expansion result: 1' in output
        with open(part, 'rb', buffering=0) as device:
            size = struct.unpack('Q', fcntl.ioctl(device, 0x80081272, bytes(8)))[0]
            device.seek(0)
            bpb = device.read(512)
        sectors = struct.unpack_from('<I', bpb, 32)[0]
        assert bpb[82:90] == b'FAT32   ' and bpb[71:82] == b'PI-DOS     '
        assert size == disk_bytes - START, 'Partition did not fill disk'
        assert 0 <= size // 512 - sectors < bpb[13], 'Filesystem did not fill partition'
        mountpoint = root / 'mounted'
        mountpoint.mkdir(exist_ok=True)
        run('mount', '-t', 'vfat', '-o', 'ro,nosuid,nodev,noexec', part, str(mountpoint))
        try:
            assert (mountpoint / 'c/KEEP.TXT').read_bytes() == KEEP, 'Lost C: file'
        finally:
            run('umount', str(mountpoint))
        with card.open('rb') as image:
            grown_mbr = image.read(512)
            assert grown_mbr[:446] == mbr[:446], 'Boot code/disk identity changed'
            assert grown_mbr[446:462] == mbr[446:462], 'BOOT partition changed'
            assert grown_mbr[470:474] == mbr[470:474], 'PI-DOS start changed'
            image.seek(1048576)
            assert image.read(len(SENTINEL)) == SENTINEL, 'BOOT data changed'
        again = run(str(probe), part)
        assert 'Expansion result: 0' in again, 'Second boot repeated growth'
        with card.open('rb') as image:
            assert image.read(512) == grown_mbr, 'Second boot modified partition table'
        print(f'PASS: {mode}, file/BOOT preservation and second-boot no-op')
    finally:
        subprocess.run(['losetup', '--detach', loop], check=True)


def main():
    if os.geteuid() != 0 or len(sys.argv) != 4:
        sys.exit('As root: test-expand.py PROBE DATA.vfat MBR.bin')
    probe, filesystem, mbr = (Path(value).resolve() for value in sys.argv[1:])
    original = mbr.read_bytes()
    assert len(original) == 512 and original[510:] == b'\x55\xaa'
    with tempfile.TemporaryDirectory(prefix='pi-dos-expand-test.') as temporary:
        root = Path(temporary)
        for mode in ('first-boot', 'resume', 'large-card', 'extra-partition', 'foreign-signature'):
            check(probe, filesystem, original, mode, root)


if __name__ == '__main__':
    main()
