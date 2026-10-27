#!/usr/bin/env bash
set -euo pipefail
root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../.." && pwd)
cd "$root"
jobs=${JOBS:-4}
case "$jobs" in ''|*[!0-9]*|0) echo 'JOBS must be positive' >&2; exit 2;; esac
mode=${1:-build}
case "$mode" in configure|build|diagnostic|source|components) ;; *) echo 'Use configure, build, diagnostic, source or components' >&2; exit 2;; esac
export PI_DOS_DIAGNOSTIC=0
image_variant=arm64
if [[ "$mode" == diagnostic ]]; then
    export PI_DOS_DIAGNOSTIC=1
    image_variant=arm64-diagnostic
fi
python3 tools/phase0/fetch.py --component buildroot
source_dir=$(python3 -c 'import json;print("output/src/"+json.load(open("sources.lock.json"))["buildroot"]["directory"])')
out="$root/output/buildroot-arm64"
mkdir -p "$out" "$root/output/logs"
args=(-C "$source_dir" "O=$out" "BR2_EXTERNAL=$root/br2-external" "BR2_DL_DIR=$root/dl/buildroot" "BR2_JLEVEL=$jobs")
if [[ "$mode" == configure || ! -f "$out/.config" ]]; then
    make "${args[@]}" pi_dos_arm64_defconfig
fi
if [[ "$mode" == configure ]]; then
    echo "Configured: $out/.config"
    exit 0
fi
# Preserve local configuration while incorporating updated package selections.
# Existing output directories predate the required internal MIDI synth library.
python3 - "$out/.config" <<'PY'
from pathlib import Path
import sys
path = Path(sys.argv[1])
text = path.read_text()
if 'BR2_PACKAGE_PI_DOS_RUNTIME=y\n' in text:
    lines = [line for line in text.splitlines()
             if not line.startswith('BR2_PACKAGE_FLUIDSYNTH=')
             and line != '# BR2_PACKAGE_FLUIDSYNTH is not set']
    path.write_text('\n'.join(lines) + '\nBR2_PACKAGE_FLUIDSYNTH=y\n')
PY
make "${args[@]}" olddefconfig > "$root/output/logs/buildroot-arm64-config.log" 2>&1
runtime_fingerprint=$(python3 - "$root" <<'PY'
from pathlib import Path
import hashlib, sys
root = Path(sys.argv[1])
digest = hashlib.sha256()
paths = [root / 'VERSION', root / 'src/common/midi_ports.h',
         root / 'src/common/reboot_shortcut.h',
         root / 'src/common/lcd_video.h',
         root / 'br2-external/package/pi-dos-runtime/pi-dos-runtime.mk']
paths += sorted((root / 'patches/dosbox-x').glob('*.patch'))
for path in paths:
    digest.update(str(path.relative_to(root)).encode())
    digest.update(path.read_bytes())
print(digest.hexdigest())
PY
)
# Reuse the exact locked DOSBox-X archive rather than downloading it twice.
archive=$(python3 -c 'import json;print(json.load(open("sources.lock.json"))["dosbox-x"]["archive"])')
if [[ -f "$root/dl/$archive" ]]; then
    mkdir -p "$root/dl/buildroot/pi-dos-runtime"
    if [[ ! -e "$root/dl/buildroot/pi-dos-runtime/$archive" ]]; then
        cp "$root/dl/$archive" "$root/dl/buildroot/pi-dos-runtime/$archive"
    fi
fi
log="$root/output/logs/buildroot-$image_variant.log"
echo "Buildroot $mode; log: $log"
if [[ "$mode" == source ]]; then
    make "${args[@]}" source > "$log" 2>&1
else
    # Local PID 1 sources are not automatically recopied by an incremental make.
    {
        if [[ ! -f "$out/.pi-dos-runtime-source" ]] || [[ $(cat "$out/.pi-dos-runtime-source") != "$runtime_fingerprint" ]]; then
            make "${args[@]}" pi-dos-runtime-dirclean
        fi
        make "${args[@]}" pi-dos-runtime
        printf '%s\n' "$runtime_fingerprint" > "$out/.pi-dos-runtime-source"
        make "${args[@]}" pi-dos-init-rebuild
        if [[ "$mode" != components ]]; then make "${args[@]}"; fi
    } > "$log" 2>&1
    if [[ "$mode" == components ]]; then
        echo "Built runtime and init components; no SD image created: $out/target"
        exit 0
    fi
    verify_args=()
    if [[ "$mode" == diagnostic ]]; then verify_args+=(--diagnostic); fi
    python3 tools/image/verify.py "${verify_args[@]}"
fi
echo "Completed Buildroot $mode: $out/images"
