#!/usr/bin/env python3
"""Fetch a pinned mTCP package and install tools, documentation and source on Z:."""
import argparse
import hashlib
import json
from pathlib import Path
import urllib.request
import zipfile

ROOT = Path(__file__).resolve().parents[2]

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--fetch', action='store_true')
    parser.add_argument('--install', type=Path)
    args = parser.parse_args()
    spec = json.loads((ROOT / 'configs/network-assets.json').read_text())
    archive = ROOT / 'dl' / spec['archive']
    if not archive.exists() and args.fetch:
        archive.parent.mkdir(parents=True, exist_ok=True)
        temporary = archive.with_suffix('.download')
        try:
            with urllib.request.urlopen(spec['url'], timeout=60) as source, temporary.open('wb') as target:
                while block := source.read(1024 * 1024):
                    target.write(block)
            if hashlib.sha256(temporary.read_bytes()).hexdigest() != spec['sha256']:
                raise RuntimeError('mTCP download checksum mismatch')
            temporary.replace(archive)
        finally:
            temporary.unlink(missing_ok=True)
    if not archive.is_file() or hashlib.sha256(archive.read_bytes()).hexdigest() != spec['sha256']:
        raise RuntimeError('Missing/changed mTCP archive. Run tools/dos/network-assets.py --fetch')
    if args.install:
        destination = args.install / 'NETWORK' / 'MTCP'
        destination.mkdir(parents=True, exist_ok=True)
        with zipfile.ZipFile(archive) as package:
            for item in package.namelist():
                relative = Path(item)
                if item.startswith('NET/mTCP/') and len(relative.parts) == 3:
                    (destination / relative.name.upper()).write_bytes(package.read(item))
            (destination / 'SOURCE.ZIP').write_bytes(package.read('SOURCE/mTCP/SOURCES.ZIP'))
            (destination / 'MANUAL.PDF').write_bytes(package.read('NET/mTCP/manual/manual.pdf'))
            (destination / 'SAMPLE.CFG').write_bytes(package.read('NET/mTCP/samples/sample.cfg'))
            (destination / 'PACKAGE.TXT').write_bytes(package.read('APPINFO/MTCP.LSM'))
    print('Verified ' + spec['version'] + ('; installed on Z:' if args.install else ''))

if __name__ == '__main__':
    main()
