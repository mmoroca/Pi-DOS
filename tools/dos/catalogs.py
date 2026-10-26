#!/usr/bin/env python3
"""Generate DOS-friendly keyboard/country/timezone pickers from packaged inputs."""
from pathlib import Path
import argparse
import re
import os

def generate(source, zoneinfo, destination):
    text = source.read_text()
    enum = text.split('enum COUNTRYNO {', 1)[1].split('};', 1)[0]
    countries = dict(re.findall(r'(\w+)\s*=\s*(\d+)', enum))
    layouts = re.findall(r'\{"([a-z0-9_]+)",\s*COUNTRYNO::(\w+)', text)
    destination.mkdir(parents=True, exist_ok=True)
    def title(name):
        return name.replace('_', ' ').replace('Candian', 'Canadian')
    def write(name, rows):
        data = ''.join(f'{key}\t{label}\r\n' for key, label in rows)
        (destination / name).write_bytes(data.encode('ascii'))
    write('KEYBOARD.LST', [('auto', 'Automatic'), ('none', 'No layout translation')] +
          sorted({(code, title(country) + f' ({code})') for code, country in layouts}, key=lambda x: x[1]))
    write('COUNTRY.LST', sorted({(code, title(name) + f' ({code})')
                               for name, code in countries.items()}, key=lambda x: x[1]))
    zones = []
    for directory, dirs, files in os.walk(zoneinfo, followlinks=True):
        if Path(directory) == zoneinfo:
            dirs[:] = [name for name in dirs if name not in ('posix', 'right')]
        for name in files:
            if name in ('localtime', 'posixrules', 'Factory'): continue
            path = Path(directory) / name
            with path.open('rb') as stream:
                if stream.read(4) != b'TZif': continue
            zone = path.relative_to(zoneinfo).as_posix()
            if len(zone) < 64: zones.append((zone, zone))
    write('TIMEZONE.LST', sorted(zones))
    if not layouts or not countries or not zones:
        raise RuntimeError('Configuration selection catalogs are empty')

if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('source', type=Path)
    parser.add_argument('zoneinfo', type=Path)
    parser.add_argument('destination', type=Path)
    args = parser.parse_args()
    generate(args.source, args.zoneinfo, args.destination)
