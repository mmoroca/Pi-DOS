#!/usr/bin/env bash
# Minimal headless shell-startup check, isolated from the user's configuration.
set -euo pipefail
root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../.." && pwd)
binary="$root/output/build/dosbox-x-sdl2/src/dosbox-x"
[[ -x "$binary" ]] || { echo 'Build DOSBox-X first with make phase0.' >&2; exit 1; }
command -v timeout >/dev/null || { echo 'GNU timeout is required.' >&2; exit 1; }
mkdir -p "$root/output/smoke"
run_dir=$(mktemp -d "$root/output/smoke/startup.XXXXXX")
echo "Run directory: $run_dir"
if timeout 20s env SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy "$binary" \
    -defaultconf -defaultmapper -nopromptfolder -defaultdir "$run_dir" \
    -fastlaunch -nomenu -nogui \
    -set 'sdl output=surface' -set 'sdl videodriver=dummy' \
    -set 'sdl waitonerror=false' \
    -c "mount c \"$run_dir\"" -c 'c:' \
    -c 'echo ARMDOS_SMOKE_OK >RESULT.TXT' -c 'exit' > "$run_dir/run.log" 2>&1; then
    if [[ -f "$run_dir/RESULT.TXT" ]] && \
        [[ "$(tr -d '\r\n' < "$run_dir/RESULT.TXT")" == ARMDOS_SMOKE_OK ]]; then
        echo 'PASS: DOS shell started, mounted C:, wrote RESULT.TXT and exited.'
    else
        echo "FAIL: DOS shell result missing or incorrect; see $run_dir/run.log" >&2
        exit 1
    fi
else
    status=$?
    tail -20 "$run_dir/run.log" >&2
    echo "FAIL: DOSBox-X exited with status $status; see $run_dir/run.log" >&2
    exit "$status"
fi
