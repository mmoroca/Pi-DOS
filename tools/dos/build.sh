#!/usr/bin/env bash
# Rebuild the bundled, real-mode 8086 DOS utility using Open Watcom C.
set -euo pipefail
root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../.." && pwd)
watcom_dir=${WATCOM:-"$root/output/tools/openwatcom"}
compiler="$watcom_dir/binl64/wcl"
[[ -x "$compiler" ]] || {
    echo 'Open Watcom not found. Set WATCOM to its installation directory.' >&2
    echo "See $root/src/doscfg/README.md for the pinned compiler and build instructions." >&2
    exit 1
}
export WATCOM="$watcom_dir"
export PATH="$watcom_dir/binl64:$PATH"
export INCLUDE="$watcom_dir/h"
out="$root/output/doscfg"
mkdir -p "$out"
cd "$out"
"$compiler" -q -bt=dos -l=dos -0 -ms -os -s -wx -i="$root/src/common" -fe=PIDOSCFG.EXE \
    "$root/src/doscfg/main.c" "$root/src/doscfg/config.c" "$root/src/common/cpu_profiles.c"
install -m 0644 PIDOSCFG.EXE "$root/br2-external/board/raspberrypi/dos/PIDOSCFG.EXE"
python3 - "$root" "$compiler" <<'PY'
import hashlib
import json
from pathlib import Path
import sys
root = Path(sys.argv[1])
paths = ['src/doscfg/main.c', 'src/doscfg/config.c', 'src/doscfg/config.h',
         'src/common/cpu_profiles.c', 'src/common/cpu_profiles.h',
         'tools/dos/build.sh', 'tools/dos/toolchain.json']
def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()
manifest = {
    'target': '16-bit DOS MZ, 8086 instruction set, small memory model, no FPU/extender',
    'source_sha256': {path: digest(root / path) for path in paths},
    'compiler_driver_sha256': digest(Path(sys.argv[2])),
    'executable_sha256': digest(root / 'br2-external/board/raspberrypi/dos/PIDOSCFG.EXE'),
}
(root / 'br2-external/board/raspberrypi/dos/PIDOSCFG.build.json').write_text(
    json.dumps(manifest, indent=2) + '\n')
PY
echo "Built $(wc -c < PIDOSCFG.EXE) bytes: Z:\\BIN\\PIDOSCFG.EXE"
