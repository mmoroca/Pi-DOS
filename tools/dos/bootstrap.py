#!/usr/bin/env python3
"""Install the pinned DOS cross-compiler inside output/tools (no system changes)."""
import hashlib
import json
from pathlib import Path
import urllib.request
import zipfile

ROOT = Path(__file__).resolve().parents[2]
lock = json.loads((ROOT / 'tools/dos/toolchain.json').read_text())
archive = ROOT / 'dl' / lock['archive']
archive.parent.mkdir(parents=True, exist_ok=True)
if not archive.exists():
    temporary = archive.with_suffix('.part')
    print('Downloading', lock['url'], flush=True)
    urllib.request.urlretrieve(lock['url'], temporary)
    temporary.replace(archive)
with archive.open('rb') as stream:
    if hashlib.file_digest(stream, 'sha256').hexdigest() != lock['sha256']:
        raise SystemExit('Open Watcom archive checksum mismatch; refusing to extract')
destination = ROOT / 'output/tools/openwatcom'
with zipfile.ZipFile(archive) as package:
    for member in package.namelist():
        if Path(member).is_absolute() or '..' in Path(member).parts:
            raise SystemExit('Unsafe compiler archive path')
    package.extractall(destination)
for binary in (destination / 'binl64').iterdir():
    if binary.is_file():
        binary.chmod(binary.stat().st_mode | 0o111)
print('Compiler installed:', destination)
