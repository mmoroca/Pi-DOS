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
python3 tools/dos/network-assets.py --fetch
python3 tools/dos/benchmark-assets.py --fetch
python3 tools/dos/demo-assets.py --fetch
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
    # Label tools receive UTF-8 after conversion from the guest DOS code page.
    locales = []
    for line in lines:
        if line.startswith('BR2_GENERATE_LOCALE='):
            locales = line.partition('=')[2].strip('"').split()
    if 'C.UTF-8' not in locales:
        locales.append('C.UTF-8')
    lines = [line for line in lines if not line.startswith('BR2_GENERATE_LOCALE=')]
    lines.append('BR2_GENERATE_LOCALE="' + ' '.join(locales) + '"')
    path.write_text('\n'.join(lines) + '\nBR2_PACKAGE_FLUIDSYNTH=y\n')
PY
make "${args[@]}" olddefconfig > "$root/output/logs/buildroot-arm64-config.log" 2>&1
runtime_fingerprint=$(python3 - "$root" <<'PY'
from pathlib import Path
import hashlib, sys
root = Path(sys.argv[1])
digest = hashlib.sha256()
paths = [root / 'VERSION', root / 'src/common/midi_ports.h',
         root / 'src/common/reboot_shortcut.h', root / 'src/common/input_options.h',
         root / 'src/common/lcd_video.h',
         root / 'br2-external/package/pi-dos-runtime/pi-dos-runtime.mk']
paths += sorted((root / 'patches/dosbox-x').glob('*.patch'))
paths += sorted((root / 'src/runtime').glob('*.h'))
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
        # Update existing kernel trees too: Buildroot's patch hook runs only
        # on extraction. Preserve compiled objects when a new patch applies
        # cleanly; re-extract if a modified patch no longer applies.
        kernel_changed=0
        if [[ -d "$out/build/linux-custom" ]]; then
            for kernel_patch in "$root"/patches/linux/*.patch; do
                [[ -f "$kernel_patch" ]] || continue
                # --force prevents GNU patch from silently ignoring -R when
                # the patch has not yet been applied.
                if patch -d "$out/build/linux-custom" -p1 --force --dry-run --reverse \
                    -i "$kernel_patch" >/dev/null 2>&1; then
                    continue
                fi
                if ! patch -d "$out/build/linux-custom" -p1 --batch --dry-run --forward \
                    -i "$kernel_patch" >/dev/null 2>&1; then
                    make "${args[@]}" linux-dirclean
                    kernel_changed=0
                    break
                fi
                patch -d "$out/build/linux-custom" -p1 --batch --forward -i "$kernel_patch"
                kernel_changed=1
            done
            if [[ "$kernel_changed" == 1 ]]; then make "${args[@]}" linux-rebuild; fi
        fi
        if [[ ! -f "$out/.pi-dos-runtime-source" ]] || [[ $(cat "$out/.pi-dos-runtime-source") != "$runtime_fingerprint" ]]; then
            make "${args[@]}" pi-dos-runtime-dirclean
        fi
        # Enabling fatlabel changes installation choices in an already-built
        # dosfstools package; Buildroot does not reinstall it on config changes.
        make "${args[@]}" dosfstools-reinstall
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
