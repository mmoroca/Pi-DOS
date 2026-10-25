#!/usr/bin/env python3
"""Exercise production FAT32 resize code without root, using sparse image files.

Arguments: host-built tests/boot/resize-fat-image.c probe, FAT32 seed containing
c/KEEP.TXT, and a Pi-DOS FAT32 MBR. This checks the resizer;
kernel partition refresh and mounting require the separate loop-device test.
"""
import errno
import os
from pathlib import Path
import struct
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[2]
HOST = ROOT / 'output/buildroot-arm64/host'
START = 526336 * 512
KEEP = b'Pi-DOS persistence during first-boot expansion\n'


def copy_sparse(source, target, offset, destination, length):
    at = offset
    while at < offset + length:
        try:
            data = os.lseek(source.fileno(), at, os.SEEK_DATA)
        except OSError as error:
            if error.errno == errno.ENXIO:
                break
            raise
        end = min(os.lseek(source.fileno(), data, os.SEEK_HOLE), offset + length)
        if data >= end:
            break
        source.seek(data)
        target.seek(destination + data - offset)
        while data < end:
            chunk = source.read(min(1024 * 1024, end - data))
            target.write(chunk)
            data += len(chunk)
        at = end


def main():
    if len(sys.argv) != 4:
        sys.exit('test-fat32-file.py PROBE DATA.vfat MBR.bin')
    probe, seed, original = (Path(x).resolve() for x in sys.argv[1:])
    original = original.read_bytes()
    assert len(original) == 512 and original[466] == 0x0c
    with tempfile.TemporaryDirectory(prefix='armdos-fat32-file.') as temporary:
        root = Path(temporary)
        for gib in (2, 64):
            size = gib * 1024**3
            card, volume = root / 'card.img', root / 'grown.vfat'
            mbr = bytearray(original)
            struct.pack_into('<I', mbr, 474, seed.stat().st_size // 512)
            with card.open('wb') as target, seed.open('rb') as source:
                target.truncate(size)
                target.write(mbr)
                target.seek(1048576)
                target.write(b'BOOT untouched')
                copy_sparse(source, target, 0, START, seed.stat().st_size)
            # Model the partition enlargement before the production FS resizer.
            grown_mbr = bytearray(mbr)
            struct.pack_into('<I', grown_mbr, 474, (size - START) // 512)
            with card.open('r+b') as image:
                image.write(grown_mbr)
            result = subprocess.run([str(probe), str(card)], capture_output=True, text=True)
            print(result.stdout + result.stderr, end='')
            result.check_returncode()
            with card.open('rb') as image:
                assert image.read(512) == grown_mbr, 'Filesystem resize altered MBR'
                image.seek(1048576)
                assert image.read(14) == b'BOOT untouched', 'Filesystem resize altered BOOT'
                image.seek(START)
                bpb = image.read(512)
            assert bpb[71:82] == b'PI-DOS     ' and bpb[82:90] == b'FAT32   '
            assert 0 <= (size - START) // 512 - struct.unpack_from('<I', bpb, 32)[0] < bpb[13]
            content = subprocess.check_output([str(HOST / 'bin/mtype'), '-i', f'{card}@@{START}', '::c/KEEP.TXT'])
            assert content == KEEP, 'C: file lost during resize'
            with card.open('rb') as source, volume.open('wb') as target:
                target.truncate(size - START)
                copy_sparse(source, target, START, 0, size - START)
            checked = subprocess.run([str(HOST / 'sbin/fsck.fat'), '-n', str(volume)], check=True, capture_output=True, text=True)
            print(checked.stdout, end='')
            again = subprocess.check_output([str(probe), str(card)], text=True)
            assert 'Expansion result: 0' in again
            print(f'PASS: {gib} GiB sparse card: FAT32 retained, C: files and BOOT preserved, fsck clean, repeated growth skipped.')
            volume.unlink()
            card.unlink()


if __name__ == '__main__':
    main()
