#!/usr/bin/env python3
r"""Fetch pinned demoscene releases and stage C:\DEMOS plus the DEMOS launcher."""
import argparse
import hashlib
import json
from pathlib import Path, PurePosixPath
import re
import tempfile
import urllib.request
import zipfile

ROOT = Path(__file__).resolve().parents[2]
PAYLOAD = ROOT / 'br2-external/board/raspberrypi/dos/demos'


def sha256(data):
    return hashlib.sha256(data).hexdigest()


def dos_text(path, text):
    path.write_bytes(text.replace('\n', '\r\n').encode('ascii'))


def install(archives, root, spec):
    destination = root / 'DEMOS'
    launcher = root / 'SYSTEM/DEMOS.BAT'
    if destination.exists() or launcher.exists():
        raise RuntimeError('Use fresh staging; DEMOS or SYSTEM/DEMOS.BAT already exists')
    destination.mkdir(parents=True)
    menu = ['@ECHO OFF', ':menu', 'CLS', 'ECHO ' + spec['version'], 'ECHO.']
    guide = [(PAYLOAD / 'README.TXT').read_text().rstrip(), '', 'THE SELECTION', '']
    sources = [spec['version'], '',
               'Original executables, data and documentation are unmodified.',
               'Complete downloaded archives are retained in each ORIG directory.',
               'Original copyright and distribution terms continue to apply.',
               'This is a local curated pack, not a blanket redistribution license.',
               'Public-image distribution review remains open. See original notices.',
               'For multi-disk releases, the first copy of a shared filename is used;',
               'all copies remain available in the unchanged original ZIPs.', '']
    for number, demo in enumerate(spec['demos'], 1):
        folder = destination / demo['id']
        original = folder / 'ORIG'
        original.mkdir(parents=True)
        for disk, item in enumerate(demo['archives'], 1):
            archive = archives[item['archive']]
            (original / f'DISK{disk}.ZIP').write_bytes(archive.read_bytes())
            with zipfile.ZipFile(archive) as package:
                for member in package.infolist():
                    relative = PurePosixPath(member.filename.replace('\\', '/').upper())
                    if (relative.is_absolute() or not relative.parts or
                            any(not re.fullmatch(r'[A-Z0-9_.-]+', part) or
                                part in ('.', '..') for part in relative.parts) or
                            relative.parts[0] == 'ORIG'):
                        raise RuntimeError('Unsafe demo archive path: ' + member.filename)
                    if member.is_dir():
                        continue
                    target = folder.joinpath(*relative.parts)
                    target.parent.mkdir(parents=True, exist_ok=True)
                    if not target.exists():
                        target.write_bytes(package.read(member))
            sources += [demo['title'] + ' - ' + demo['group'],
                        item['url'], 'SHA256 ' + item['sha256'], '']
        for command in demo['commands']:
            if not (folder / command).is_file():
                raise RuntimeError('Missing demo program: ' + demo['id'] + '/' + command)
        label = f"{demo['title']} - {demo['group']} ({demo['year']})"
        menu.append(f'ECHO {number}  {label}')
        guide += [f'{number}. {label}', demo['description'],
                  '   ' + demo['notes'], '   C:\\DEMOS\\' + demo['id'], '']
    keys = ''.join(str(i) for i in range(1, len(spec['demos']) + 1))
    menu += ['ECHO.', 'ECHO H  Read the guide                  Q  Return to DOS',
             'ECHO.', 'ECHO Select Sound Blaster in the original demo setup screens.',
             'ECHO The menu uses your current Pi-DOS CPU, video and audio settings.',
             'ECHO.', f'Z:\\DOS\\CHOICE.COM /N /C:{keys}HQ Select demo:',
             f'IF ERRORLEVEL {len(keys) + 2} GOTO end',
             f'IF ERRORLEVEL {len(keys) + 1} GOTO help']
    for number in range(len(keys), 0, -1):
        menu.append(f'IF ERRORLEVEL {number} GOTO demo{number}')
    menu += ['GOTO menu']
    for number, demo in enumerate(spec['demos'], 1):
        menu += [f':demo{number}', 'PUSHD C:\\DEMOS\\' + demo['id']]
        menu += demo['commands']
        menu += ['POPD', 'ECHO.', 'PAUSE', 'GOTO menu']
    menu += [':help', 'MORE < C:\\DEMOS\\README.TXT', 'PAUSE', 'GOTO menu', ':end']
    dos_text(destination / 'MENU.BAT', '\n'.join(menu) + '\n')
    dos_text(destination / 'README.TXT', '\n'.join(guide) + '\n')
    dos_text(destination / 'SOURCE.TXT', '\n'.join(sources) + '\n')
    launcher.parent.mkdir(parents=True, exist_ok=True)
    dos_text(launcher, (PAYLOAD / 'DEMOS.BAT').read_text())
    files = {path.relative_to(destination).as_posix(): sha256(path.read_bytes())
             for path in sorted(destination.rglob('*')) if path.is_file()}
    (destination / 'FILES.JSN').write_text(
        json.dumps({'version': spec['version'], 'files': files}, indent=2) + '\n')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--fetch', action='store_true')
    parser.add_argument('--install', type=Path, help='C-drive staging root (DEMOS must not exist)')
    parser.add_argument('--pack', type=Path, help='write a ready-to-copy ZIP for an existing card')
    args = parser.parse_args()
    spec = json.loads((ROOT / 'configs/demo-assets.json').read_text())
    if not 1 <= len(spec['demos']) <= 9:
        raise RuntimeError('The numbered menu supports one to nine demos')
    archives = {}
    for demo in spec['demos']:
        if not re.fullmatch(r'[A-Z0-9_]{1,8}', demo['id']):
            raise RuntimeError('Invalid DOS demo directory: ' + demo['id'])
        for item in demo['archives']:
            archive = ROOT / 'dl' / item['archive']
            if not archive.exists() and args.fetch:
                archive.parent.mkdir(parents=True, exist_ok=True)
                temporary = archive.with_suffix('.download')
                try:
                    with urllib.request.urlopen(item['url'], timeout=60) as source, temporary.open('wb') as target:
                        while block := source.read(1024 * 1024):
                            target.write(block)
                    if sha256(temporary.read_bytes()) != item['sha256']:
                        raise RuntimeError('Demo checksum mismatch: ' + item['archive'])
                    temporary.replace(archive)
                finally:
                    temporary.unlink(missing_ok=True)
            if not archive.is_file() or sha256(archive.read_bytes()) != item['sha256']:
                raise RuntimeError('Missing/changed ' + item['archive'] +
                                   '; run tools/dos/demo-assets.py --fetch')
            archives[item['archive']] = archive
    if args.install:
        install(archives, args.install, spec)
    if args.pack:
        args.pack.parent.mkdir(parents=True, exist_ok=True)
        with tempfile.TemporaryDirectory(prefix='pi-dos-demos-') as temporary:
            staging = Path(temporary)
            install(archives, staging, spec)
            with zipfile.ZipFile(args.pack, 'w', compression=zipfile.ZIP_DEFLATED) as package:
                for path in sorted(staging.rglob('*')):
                    if path.is_file():
                        package.write(path, path.relative_to(staging).as_posix())
        print('Created ' + str(args.pack))
    print('Verified ' + spec['version'] + ('; staged C:\\DEMOS and launcher' if args.install else ''))


if __name__ == '__main__':
    main()
