#!/usr/bin/env python3
"""Stage hash-pinned original Windows 3.1 audio drivers, without modifying them."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import subprocess
import tempfile
import urllib.request
import zipfile

ROOT = Path(__file__).resolve().parents[2]


def contents(path):
    """unzip also handles the legacy PKZIP implode method in Creative SFXs."""
    with zipfile.ZipFile(path) as package:
        for item in package.infolist():
            if item.is_dir():
                continue
            if not re.fullmatch(r'[A-Za-z0-9_!-]{1,8}(?:\.[A-Za-z0-9_!-]{1,3})?', item.filename):
                raise ValueError(f'Not a flat DOS driver disk: {item.filename}')
            if item.file_size > 16 * 1024 * 1024 or item.flag_bits & 1:
                raise ValueError('Oversized or encrypted driver payload')
            if item.compress_type not in (0, 8):
                data = subprocess.run(['unzip', '-p', str(path), item.filename],
                                      check=True, stdout=subprocess.PIPE).stdout
            else:
                data = package.read(item)
            if len(data) != item.file_size or (zipfile.crc32(data) & 0xffffffff) != item.CRC:
                raise ValueError('Corrupt driver payload')
            yield item.filename.upper(), data


def prepare(cache, disk, manifest_path, fetch=False):
    sources = json.loads((ROOT / 'configs/windows-audio.json').read_text())
    catalog = []
    for driver in sources['drivers']:
        if not driver['enabled']:
            continue
        archive = cache / driver['archive']
        if not archive.exists():
            if not fetch:
                raise ValueError(f'Missing {archive}. Run tools/windows/package-audio.py --fetch first.')
            cache.mkdir(parents=True, exist_ok=True)
            with urllib.request.urlopen(driver['url'], timeout=60) as response:
                data = response.read(16 * 1024 * 1024 + 1)
            if len(data) > 16 * 1024 * 1024 or hashlib.sha256(data).hexdigest() != driver['sha256']:
                raise ValueError('Audio archive does not match the pinned release')
            archive.write_bytes(data)
        if hashlib.sha256(archive.read_bytes()).hexdigest() != driver['sha256']:
            raise ValueError(f'Audio archive hash mismatch: {archive}')
        folder = disk / 'AUDIO' / driver['id']
        folder.mkdir(parents=True, exist_ok=True)
        path = archive
        if driver.get('nested'):
            with zipfile.ZipFile(archive) as outer:
                matches = [n for n in outer.namelist() if n.lower() == driver['nested'].lower()]
                if len(matches) != 1:
                    raise ValueError('Missing nested vendor driver disk')
                nested = outer.read(matches[0])
            with tempfile.NamedTemporaryFile(suffix='.exe') as temporary:
                temporary.write(nested)
                temporary.flush()
                payloads = list(contents(Path(temporary.name)))
        else:
            payloads = list(contents(path))
        files = []
        seen = set()
        for name, data in payloads:
            if name in seen:
                raise ValueError('Duplicate DOS payload name')
            seen.add(name)
            (folder / name).write_bytes(data)
            files.append(dict(name=name, size=len(data), sha256=hashlib.sha256(data).hexdigest()))
        expected = ['INSTALL.EXE'] if driver['method'] == 'dos' else [driver['wave'], 'OEMSETUP.INF']
        expected += [name for name in (driver['fm'], driver['aux']) if name]
        for name in expected:
            if name not in seen:
                raise ValueError(f'Missing vendor audio payload: {name}')
            if name.endswith(('.DRV', '.EXE')) and not (folder / name).read_bytes().startswith(b'MZ'):
                raise ValueError(f'Not a DOS/Win16 executable: {name}')
        if driver['method'] == 'native':
            oem = (folder / 'OEMSETUP.INF').read_text(encoding='cp437').lower()
            for name in [driver['wave'], driver['fm'], driver['aux']]:
                if name and name.lower() not in oem:
                    raise ValueError('Driver selection disagrees with vendor OEMSETUP.INF')
        catalog.append(dict(driver, files=files))
    manifest_path.write_text(json.dumps(dict(drivers=catalog, builtin=sources['builtin'],
                                             unavailable=sources['unavailable']), indent=2) + '\n')
    code = ['/* Unmodified vendor payloads. Generated from windows-audio.json. */',
            'static const AudioFile audio_files[] = {']
    offset = 0
    profiles = []
    for driver in catalog:
        for file in driver['files']:
            code.append('    {"%s",%dUL},' % (file['name'], file['size']))
        profiles.append('    {"%s","%s",%d,%d,%d,%d,"%s","%s","%s"},' %
                        (driver['id'], driver['label'], driver['card'], driver['method'] == 'dos',
                         offset, len(driver['files']), driver['wave'], driver['fm'], driver['aux']))
        offset += len(driver['files'])
    code.extend(['};', 'static const AudioProfile audio_profiles[] = {', *profiles, '};',
                 '#define AUDIO_PROFILE_COUNT (sizeof(audio_profiles)/sizeof(audio_profiles[0]))', ''])
    return '\n'.join(code)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--fetch', action='store_true')
    parser.add_argument('--output', type=Path, default=ROOT / 'output/windows-video')
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=True)
    try:
        code = prepare(ROOT / 'output/windows-audio', args.output / 'DISK', args.output / 'audio.json', args.fetch)
    except (OSError, ValueError, zipfile.BadZipFile, subprocess.CalledProcessError) as error:
        parser.exit(1, f'Error: {error}\n')
    (args.output / 'audio_catalog.h').write_text(code)
    print('Prepared original SB2, SB Pro 2, SB16, Gravis GF1 and PC speaker packages')


if __name__ == '__main__':
    main()
