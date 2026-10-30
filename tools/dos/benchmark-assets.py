#!/usr/bin/env python3
r"""Fetch pinned benchmarks and stage C:\SYSTEM\BENCH for the SD image."""
import argparse
import hashlib
import json
from pathlib import Path, PurePosixPath
import re
import subprocess
import tempfile
import urllib.request
import zipfile

ROOT = Path(__file__).resolve().parents[2]
PAYLOAD = ROOT / 'br2-external/board/raspberrypi/dos/bench'


def sha256(data):
    return hashlib.sha256(data).hexdigest()


def write_file(destination, name, data):
    relative = PurePosixPath(name.upper())
    if (relative.is_absolute() or not relative.parts or
            any(not re.fullmatch(r'[A-Z0-9_-]{1,8}(?:\.[A-Z0-9_-]{1,3})?', part)
                for part in relative.parts)):
        raise RuntimeError('Unsafe/non-DOS benchmark path: ' + name)
    path = destination.joinpath(*relative.parts)
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_bytes(data)


def original_game(archive, destination, game):
    # DEICE joins the volumes into an SFX. Read its archive without executing it.
    with zipfile.ZipFile(archive) as package:
        if game == 'doom':
            resource = package.read('DOOMS_19.1') + package.read('DOOMS_19.2')
        else:
            resource = package.read('resource.1')
    with tempfile.TemporaryDirectory(prefix='pi-dos-bench-') as temporary:
        sfx = Path(temporary) / 'resource.bin'
        sfx.write_bytes(resource)
        names = subprocess.check_output(['bsdtar', '-tf', str(sfx)], text=True).splitlines()
        for name in names:
            if not name.endswith('/'):
                data = subprocess.check_output(['bsdtar', '-xOf', str(sfx), name])
                write_file(destination / game.upper(), name, data)
    # Retain the complete original distribution as well as installed files.
    write_file(destination / game.upper(), 'ORIG.ZIP', archive.read_bytes())


def install(archives, destination, spec):
    destination.mkdir(parents=True, exist_ok=True)
    for game in ('doom', 'quake'):
        original_game(archives[game], destination, game)
    with zipfile.ZipFile(archives['phil']) as package:
        for prefix, folder in [('MARKS/3DBENCH2/', '3DBENCH'),
                               ('MARKS/CHRISB/', 'CHRIS'),
                               ('MARKS/PCPBENCH/', 'PCPBENCH')]:
            for name in package.namelist():
                if name.startswith(prefix) and not name.endswith('/'):
                    write_file(destination / folder, name[len(prefix):], package.read(name))
        write_file(destination, 'CHOICE.EXE', package.read('choice.exe'))
        write_file(destination, 'PHIL.TXT', package.read('README.TXT'))
        for name in ('MIN.CFG', 'MAX.CFG', 'DOOMFPS1.EXE', 'DOOMFPS1.C',
                     'DOOMFPS2.COM', 'DOOMFPS2.C', 'MAKEFILE'):
            write_file(destination / 'DOOM', name, package.read('MARKS/DOOMS/' + name))
        for name in ('MODE0.CFG', 'MODE10.CFG', 'MODE12.CFG'):
            write_file(destination / 'QUAKE/ID1', name,
                       package.read('MARKS/QUAKES/ID1/' + name))
        # The full original shareware releases must run the same benchmark builds.
        for source, target in [('MARKS/DOOMS/DOOM.EXE', 'DOOM/DOOM.EXE'),
                               ('MARKS/DOOMS/DOOM1.WAD', 'DOOM/DOOM1.WAD'),
                               ('MARKS/QUAKES/QUAKE.EXE', 'QUAKE/QUAKE.EXE'),
                               ('MARKS/QUAKES/ID1/PAK0.PAK', 'QUAKE/ID1/PAK0.PAK')]:
            if package.read(source) != (destination / target).read_bytes():
                raise RuntimeError('Original game differs from benchmark pack: ' + target)
    for name in ('MENU.BAT', 'README.TXT'):
        write_file(destination, name, (PAYLOAD / name).read_text().replace('\n', '\r\n').encode('ascii'))
    notice = (spec['version'] + '\n\n'
              'Benchmark selection/menu: Pi-DOS. Original compilation: Phil\'s Computer Lab.\n'
              'Original executables and game data are unmodified.\n'
              'Doom 1.9 and Quake 1.06 are SHAREWARE, not public-domain software.\n'
              'Their complete original installers are retained as DOOM/ORIG.ZIP\n'
              'and QUAKE/ORIG.ZIP, with all installed documentation beside the games.\n'
              'Read QUAKE/SLICNSE.TXT and the original game documentation.\n'
              'Superscape, Chris Dial and PC Player retain their component rights.\n'
              'DOOMFPS: Thomas Perl; source accompanies both supplied helpers.\n'
              'Availability in Phil\'s compilation does not establish redistribution\n'
              'permission for every component. Pi-DOS public-release review remains open.\n\n'
              'Sources (SHA-256 checked by the build):\n')
    for item in spec['archives']:
        notice += item['archive'] + '\n' + item['url'] + '\nSHA256 ' + item['sha256'] + '\n\n'
    write_file(destination, 'SOURCE.TXT', notice.replace('\n', '\r\n').encode('ascii'))
    files = {path.relative_to(destination).as_posix(): sha256(path.read_bytes())
             for path in sorted(destination.rglob('*'))
             if path.is_file() and path.name != 'FILES.JSN'}
    (destination / 'FILES.JSN').write_text(json.dumps({'version': spec['version'], 'files': files},
                                                   indent=2) + '\n')
    # C:\SYSTEM is already on PATH. CALL and PUSHD retain the caller's directory.
    (destination.parent / 'BENCH.BAT').write_bytes(
        (PAYLOAD / 'BENCH.BAT').read_text().replace('\n', '\r\n').encode('ascii'))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--fetch', action='store_true')
    parser.add_argument('--install', type=Path, help='fresh SYSTEM/BENCH staging directory')
    args = parser.parse_args()
    spec = json.loads((ROOT / 'configs/benchmark-assets.json').read_text())
    archives = {}
    for item in spec['archives']:
        archive = ROOT / 'dl' / item['archive']
        if not archive.exists() and args.fetch:
            archive.parent.mkdir(parents=True, exist_ok=True)
            temporary = archive.with_suffix('.download')
            try:
                with urllib.request.urlopen(item['url'], timeout=60) as source, temporary.open('wb') as target:
                    while block := source.read(1024 * 1024):
                        target.write(block)
                if sha256(temporary.read_bytes()) != item['sha256']:
                    raise RuntimeError('Benchmark download checksum mismatch: ' + item['archive'])
                temporary.replace(archive)
            finally:
                temporary.unlink(missing_ok=True)
        if not archive.is_file() or sha256(archive.read_bytes()) != item['sha256']:
            raise RuntimeError('Missing/changed ' + item['archive'] +
                               '; run tools/dos/benchmark-assets.py --fetch')
        archives[item['id']] = archive
    if args.install:
        install(archives, args.install, spec)
    print('Verified ' + spec['version'] + ('; staged on C:' if args.install else ''))


if __name__ == '__main__':
    main()
