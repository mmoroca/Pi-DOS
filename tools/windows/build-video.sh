#!/usr/bin/env bash
# Build a native Windows 3.1 EXE + CPL with user-supplied vendor payloads.
set -euo pipefail
root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../.." && pwd)
watcom_dir=${WATCOM:-"$root/output/tools/openwatcom"}
out="$root/output/windows-video"
if [[ $# -ne 1 || -z "$1" ]]; then
    echo "Usage: $0 /path/to/DRIVERS (containing S3/ and ET4/)" >&2
    exit 2
fi
for tool in wcc wlink wrc; do
    [[ -x "$watcom_dir/binl64/$tool" ]] || {
        echo "Open Watcom not found. Set WATCOM to its installation directory." >&2
        exit 1
    }
done
export WATCOM="$watcom_dir"
export PATH="$watcom_dir/binl64:$PATH"
export INCLUDE="$watcom_dir/h/win:$watcom_dir/h"
python3 "$root/tools/windows/package-video.py" "$1" --output "$out"
cd "$out"
# Original, code-drawn 16-colour Windows icon; no external artwork dependency.
python3 - <<'PY'
import struct
from pathlib import Path
pixels = [[0 for _ in range(32)] for _ in range(32)]
for y in range(3, 25):
    for x in range(2, 30):
        pixels[y][x] = 7 if x in (2, 29) or y in (3, 24) else 8
for y in range(6, 21):
    for x in range(5, 27):
        pixels[y][x] = 9 if y < 10 else (3 if x < 16 else 11)
for y in range(25, 28):
    for x in range(13, 19): pixels[y][x] = 8
for x in range(9, 23): pixels[28][x] = 7
palette = bytes.fromhex('000000 000080 008000 008080 800000 800080 808000 c0c0c0 '
                        '808080 0000ff 00ff00 00ffff ff0000 ff00ff ffff00 ffffff')
palette = b''.join(palette[i:i+3] + b'\0' for i in range(0, 48, 3))
bitmap = bytes((row[x] << 4) | row[x+1] for row in reversed(pixels) for x in range(0, 32, 2))
mask = bytes(128)
dib = struct.pack('<IiiHHIIiiII', 40, 32, 64, 1, 4, 0, len(bitmap)+len(mask), 0, 0, 16, 0)
data = dib + palette + bitmap + mask
Path('video.ico').write_bytes(struct.pack('<HHH', 0, 1, 1) +
    struct.pack('<BBBBHHII', 32, 32, 16, 0, 1, 4, len(data), 22) + data)
PY
wcc -q -bt=windows -3 -ml -zu -bd -zc -os -wx -i="$root/src/winvideo" -i="$out" \
    -fo=video.obj "$root/src/winvideo/video.c"
wcc -q -bt=windows -3 -ml -zu -bd -zc -os -wx -i="$root/src/winvideo" \
    -fo=transaction.obj "$root/src/winvideo/transaction.c"
wcc -q -bt=windows -3 -ml -zw -os -wx -fo=setup.obj "$root/src/winvideo/setup.c"
cat > video.lnk <<'EOF'
system windows_dll initinstance memory
option quiet
option oneautodata
option heapsize=4096
option map=video.map
name DISK/PIVIDEO.CPL
file video.obj,transaction.obj
library windows
export WEP resident
EOF
wlink @video.lnk
wrc -q -r -bt=windows -30 -i="$root/src/winvideo" -i="$out" \
    -fo=video.res "$root/src/winvideo/video.rc"
wrc -q -s0 video.res DISK/PIVIDEO.CPL
cat > setup.lnk <<'EOF'
system windows
option quiet
option stack=8192
option heapsize=4096
option map=setup.map
name DISK/SETUP.EXE
file setup.obj
library windows
EOF
wlink @setup.lnk
cp "$root/src/winvideo/README.TXT" DISK/README.TXT
python3 - "$root" <<'PY'
from pathlib import Path
import hashlib
import json
import zipfile
import sys
root = Path(sys.argv[1])
out = root / 'output/windows-video'
disk = out / 'DISK'
manifest = json.loads((out / 'catalog.json').read_text())
manifest['package_sha256'] = {str(p.relative_to(disk)): hashlib.sha256(p.read_bytes()).hexdigest()
                              for p in sorted(disk.rglob('*')) if p.is_file()}
manifest['source_code_sha256'] = {str(p.relative_to(root)): hashlib.sha256(p.read_bytes()).hexdigest()
                                 for folder in ('src/winvideo', 'tools/windows')
                                 for p in sorted((root / folder).glob('*')) if p.is_file()}
(out / 'build.json').write_text(json.dumps(manifest, indent=2) + '\n')
with zipfile.ZipFile(out / 'PIDOSVID.ZIP', 'w', zipfile.ZIP_DEFLATED) as pack:
    for p in sorted(disk.rglob('*')):
        if p.is_file(): pack.write(p, str(Path('PIDOSVID') / p.relative_to(disk)))
print(f'Built {out / "PIDOSVID.ZIP"}')
PY
