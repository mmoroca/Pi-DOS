#!/usr/bin/env bash
set -euo pipefail
root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../.." && pwd)
cd "$root"
jobs=${JOBS:-4}
case "$jobs" in ''|*[!0-9]*|0) echo 'JOBS must be positive' >&2; exit 2;; esac
mode=${1:-build}
case "$mode" in configure|build|diagnostic|source) ;; *) echo 'Use configure, build, diagnostic or source' >&2; exit 2;; esac
export ARMDOS_DIAGNOSTIC=0
image_variant=arm64
if [[ "$mode" == diagnostic ]]; then
    export ARMDOS_DIAGNOSTIC=1
    image_variant=arm64-diagnostic
fi
python3 tools/phase0/fetch.py --component buildroot
source_dir=$(python3 -c 'import json;print("output/src/"+json.load(open("sources.lock.json"))["buildroot"]["directory"])')
out="$root/output/buildroot-arm64"
mkdir -p "$out" "$root/output/logs"
args=(-C "$source_dir" "O=$out" "BR2_EXTERNAL=$root/br2-external" "BR2_DL_DIR=$root/dl/buildroot" "BR2_JLEVEL=$jobs")
if [[ "$mode" == configure || ! -f "$out/.config" ]]; then
    make "${args[@]}" armdos_arm64_defconfig
fi
if [[ "$mode" == configure ]]; then
    echo "Configured: $out/.config"
    exit 0
fi
# Preserve local configuration while incorporating updated package selections.
make "${args[@]}" olddefconfig > "$root/output/logs/buildroot-arm64-config.log" 2>&1
# Reuse the exact locked DOSBox-X archive rather than downloading it twice.
archive=$(python3 -c 'import json;print(json.load(open("sources.lock.json"))["dosbox-x"]["archive"])')
if [[ -f "$root/dl/$archive" ]]; then
    mkdir -p "$root/dl/buildroot/armdos-runtime"
    if [[ ! -e "$root/dl/buildroot/armdos-runtime/$archive" ]]; then
        cp "$root/dl/$archive" "$root/dl/buildroot/armdos-runtime/$archive"
    fi
fi
log="$root/output/logs/buildroot-$image_variant.log"
echo "Buildroot $mode; log: $log"
if [[ "$mode" == source ]]; then
    make "${args[@]}" source > "$log" 2>&1
else
    # Local PID 1 sources are not automatically recopied by an incremental make.
    {
        make "${args[@]}" armdos-init-rebuild
        make "${args[@]}"
    } > "$log" 2>&1
    verify_args=()
    if [[ "$mode" == diagnostic ]]; then verify_args+=(--diagnostic); fi
    python3 tools/image/verify.py "${verify_args[@]}"
fi
echo "Completed Buildroot $mode: $out/images"
