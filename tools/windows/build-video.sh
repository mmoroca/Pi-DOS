#!/usr/bin/env bash
# Build the shared Windows 3.1 driver/configuration application.
set -euo pipefail
root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../.." && pwd)
watcom_dir=${WATCOM:-"$root/output/tools/openwatcom"}
out="$root/output/windows-video"
if [[ $# -ne 1 || -z "$1" ]]; then
    echo "Usage: $0 /path/to/DRIVERS (containing S3/ and ET4/)" >&2
    exit 2
fi
for tool in wcc wcl wlink wrc; do
    [[ -x "$watcom_dir/binl64/$tool" ]] || {
        echo "Open Watcom not found. Set WATCOM to its installation directory." >&2
        exit 1
    }
done
export WATCOM="$watcom_dir"
export PATH="$watcom_dir/binl64:$PATH"
export INCLUDE="$watcom_dir/h/win:$watcom_dir/h"
python3 "$root/tools/windows/package-video.py" "$1" --output "$out"
python3 "$root/tools/windows/package-audio.py" --output "$out"
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
for name in control audio audio_ini config_ui applet; do
    wcc -q -bt=windows -3 -ml -zu -bd -zc -os -wx \
        -i="$root/src/winvideo" -i="$root/src/doscfg" -i="$root/src/common" -i="$out" \
        -fo="$name.obj" "$root/src/winvideo/$name.c"
done
for name in config forms; do
    wcc -q -bt=windows -3 -ml -zu -bd -zc -os -wx -i="$root/src/common" \
        -fo="dos_$name.obj" "$root/src/doscfg/$name.c"
done
for name in cpu_profiles video_profiles midi_profiles; do
    wcc -q -bt=windows -3 -ml -zu -bd -zc -os -wx \
        -fo="$name.obj" "$root/src/common/$name.c"
done
wcc -q -bt=windows -3 -ml -zu -bd -zc -os -wx -i="$root/src/winvideo" \
    -fo=transaction.obj "$root/src/winvideo/transaction.c"
wcc -q -bt=windows -3 -ml -zw -os -wx -fo=setup.obj "$root/src/winvideo/setup.c"
cat > video.lnk <<'EOF'
system windows_dll initinstance memory
option quiet
option oneautodata
option heapsize=4096
option map=video.map
name DISK/PIDOS.CPL
file video.obj,transaction.obj,control.obj,audio.obj,audio_ini.obj,config_ui.obj,applet.obj
file dos_config.obj,dos_forms.obj,cpu_profiles.obj,video_profiles.obj,midi_profiles.obj
library windows
export WEP resident
EOF
wlink @video.lnk
wrc -q -r -bt=windows -30 -i="$root/src/winvideo" -i="$out" \
    -fo=video.res "$root/src/winvideo/video.rc"
wrc -q -s0 video.res DISK/PIDOS.CPL
# Give Setup its own module identity. Loading a newer CPL by path alone can
# return an older DLL already cached under the same name by Windows 3.1.
sed -e 's@name DISK/PIDOS.CPL@name DISK/PIDINST.DLL@' \
    -e 's@option map=video.map@option map=installer.map@' video.lnk > installer.lnk
wlink @installer.lnk
wrc -q -s0 video.res DISK/PIDINST.DLL
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
wrc -q -r -bt=windows -30 -i="$root/src/winvideo" \
    -fo=setup.res "$root/src/winvideo/setup.rc"
wrc -q -s0 setup.res DISK/SETUP.EXE
wcl -q -bt=dos -l=dos -0 -ms -os -s -wx -fe=DISK/AUDIO/GUS/PVRUN.EXE \
    "$root/src/winvideo/audio_run.c"
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
manifest['audio'] = json.loads((out / 'audio.json').read_text())
manifest['package_sha256'] = {str(p.relative_to(disk)): hashlib.sha256(p.read_bytes()).hexdigest()
                              for p in sorted(disk.rglob('*')) if p.is_file()}
manifest['audio_policy_sha256'] = hashlib.sha256((root / 'configs/windows-audio.json').read_bytes()).hexdigest()
manifest['source_code_sha256'] = {str(p.relative_to(root)): hashlib.sha256(p.read_bytes()).hexdigest()
                                 for folder in ('src/winvideo', 'tools/windows', 'src/doscfg', 'src/common')
                                 for p in sorted((root / folder).glob('*')) if p.is_file()}
(out / 'build.json').write_text(json.dumps(manifest, indent=2) + '\n')
with zipfile.ZipFile(out / 'PIDOSVID.ZIP', 'w', zipfile.ZIP_DEFLATED) as pack:
    for p in sorted(disk.rglob('*')):
        if p.is_file(): pack.write(p, str(Path('PIDOSDRV') / p.relative_to(disk)))
import shutil
shutil.copyfile(out / 'PIDOSVID.ZIP', out / 'PIDOSDRV.ZIP')
print(f'Built {out / "PIDOSDRV.ZIP"} (PIDOSVID.ZIP is a compatibility copy)')
PY
