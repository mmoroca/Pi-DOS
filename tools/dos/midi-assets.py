#!/usr/bin/env python3
"""Stage the user's MIDI assets locally; binary ROMs/SoundFonts stay out of git."""
import argparse
import hashlib
import json
from pathlib import Path
import shutil
import tempfile

ROOT = Path(__file__).resolve().parents[2]
CACHE = ROOT / 'output/midi-assets'

def verify(path, item):
    if not path.is_file() or path.stat().st_size != item['bytes']:
        raise RuntimeError(f'Missing or wrong-size MIDI asset: {path}')
    with path.open('rb') as stream:
        digest = hashlib.file_digest(stream, 'sha256').hexdigest()
    if digest != item['sha256']:
        raise RuntimeError(f'MIDI asset checksum mismatch: {path}')

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source', type=Path, help='ScummVM directory containing extra/ and soundfonts/')
    parser.add_argument('--install', type=Path, help='Copy verified cached assets to this directory')
    args = parser.parse_args()
    manifest = json.loads((ROOT / 'configs/midi-assets.json').read_text())
    for item in manifest['files']:
        cached = CACHE / item['destination']
        if args.source:
            source = args.source / item['source']
            verify(source, item)
            cached.parent.mkdir(parents=True, exist_ok=True)
            with tempfile.NamedTemporaryFile(dir=cached.parent, delete=False) as stream:
                temporary = Path(stream.name)
            try:
                shutil.copyfile(source, temporary)
                verify(temporary, item)
                temporary.replace(cached)
                cached.chmod(0o644)
            finally:
                temporary.unlink(missing_ok=True)
        verify(cached, item)
        if args.install:
            target = args.install / item['destination']
            target.parent.mkdir(parents=True, exist_ok=True)
            shutil.copyfile(cached, target)
            target.chmod(0o644)
    print('Verified six MIDI assets' + (f'; installed in {args.install}' if args.install else f' in {CACHE}'))

if __name__ == '__main__':
    try:
        main()
    except (OSError, RuntimeError) as error:
        raise SystemExit(f'{error}\nImport with: python3 tools/dos/midi-assets.py --source /path/to/scummvm')
