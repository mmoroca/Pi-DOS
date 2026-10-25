#!/usr/bin/env bash
# Run the Phase 0 fixtures using the normal interpreter or explicit dynrec core.
set -euo pipefail
root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../.." && pwd)
binary="$root/output/build/dosbox-x-sdl2/src/dosbox-x"
cpu_core=${CPU_CORE:-normal}
verify_dynrec=${VERIFY_DYNREC:-0}
case "$cpu_core" in
    normal|dynamic_rec) ;;
    *) echo 'CPU_CORE must be normal or dynamic_rec.' >&2; exit 2;;
esac
case "$verify_dynrec" in
    0|1) ;;
    *) echo 'VERIFY_DYNREC must be 0 or 1.' >&2; exit 2;;
esac
if [[ "$verify_dynrec" == 1 && "$cpu_core" != dynamic_rec ]]; then
    echo 'VERIFY_DYNREC=1 requires CPU_CORE=dynamic_rec.' >&2
    exit 2
fi
[[ -x "$binary" ]] || { echo 'Build DOSBox-X first with make phase0.' >&2; exit 1; }
if [[ "$cpu_core" == dynamic_rec ]] && \
    ! grep -q '^#define C_DYNREC 1' "$root/output/build/dosbox-x-sdl2/config.h"; then
    echo 'This build does not include the dynrec core.' >&2
    exit 1
fi
for tool in timeout nasm python3; do
    command -v "$tool" >/dev/null || { echo "Required tool missing: $tool" >&2; exit 1; }
done
runner=("$binary")
time_limit=20s
if [[ "$verify_dynrec" == 1 ]]; then
    command -v gdb >/dev/null || { echo 'Dynrec verification requires GDB.' >&2; exit 1; }
    runner=(gdb -q -nx --batch --return-child-result -x "$root/tools/phase0/dynrec.gdb" --args "$binary")
    time_limit=60s
fi
mkdir -p "$root/output/smoke"
run_dir=$(mktemp -d "$root/output/smoke/programs.XXXXXX")
echo "Run directory: $run_dir"
echo "Requested CPU core: $cpu_core"
nasm -f bin "$root/tests/dos/comtest.asm" -o "$run_dir/COMTEST.COM"
nasm -f bin "$root/tests/dos/exetest.asm" -o "$run_dir/EXETEST.EXE"
# DOS batch input uses CRLF, independent of the source checkout's line endings.
python3 - "$root/tests/dos/run.bat" "$run_dir/RUN.BAT" <<'PY'
from pathlib import Path
import sys
Path(sys.argv[2]).write_bytes(Path(sys.argv[1]).read_bytes().replace(b'\r\n', b'\n').replace(b'\n', b'\r\n'))
PY
if timeout "$time_limit" env SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy "${runner[@]}" \
    -defaultconf -defaultmapper -nopromptfolder -defaultdir "$run_dir" \
    -fastlaunch -nomenu -nogui \
    -set 'sdl output=surface' -set 'sdl videodriver=dummy' \
    -set 'sdl waitonerror=false' -set "cpu core=$cpu_core" \
    -c "mount c \"$run_dir\"" -c 'c:' -c 'call run.bat' -c 'exit' \
    > "$run_dir/run.log" 2>&1; then
    python3 - "$run_dir" "$cpu_core" "$verify_dynrec" <<'PY'
from pathlib import Path
import sys

run = Path(sys.argv[1])
expected = {
    'COM.OUT': b'PI_DOS_COM_OK\r\n',
    'EXE.OUT': b'PI_DOS_EXE_OK\r\n',
    'BAT.OUT': b'PI_DOS_BAT_OK\r\n',
    'WORK/SOURCE.TXT': b'PI_DOS_FILE_OK\r\n',
    'WORK/COPY.TXT': b'PI_DOS_FILE_OK\r\n',
    'RESULT.TXT': b'PI_DOS_PROGRAMS_OK\r\n',
}
failures = [name for name, data in expected.items()
            if not (run / name).is_file() or (run / name).read_bytes() != data]
listing = run / 'WORK/DIR.TXT'
if not listing.is_file() or b'COPY' not in listing.read_bytes().upper():
    failures.append('WORK/DIR.TXT')
if (run / 'FAIL.TXT').exists():
    failures.append('DOS batch failure branch')
log = (run / 'run.log').read_text(errors='replace')
if 'Unknown core type' in log:
    failures.append('requested CPU core rejected')
if sys.argv[3] == '1':
    for marker in ('PI_DOS_DYNREC_ENTRY_REACHED',
                   'PI_DOS_DYNREC_BLOCK_RETURN_REACHED',
                   'PI_DOS_DYNREC_DECODER_MATCH=1'):
        if marker not in log:
            failures.append(marker)
if failures:
    sys.exit(f"FAIL: {', '.join(failures)}; inspect {run}/run.log")
core = 'normal interpreter' if sys.argv[2] == 'normal' else 'requested dynamic_rec core'
print(f'PASS: COM, relocated MZ EXE, exit codes, BAT variables, DIR/CD/COPY ({core}).')
if sys.argv[3] == '1':
    print('PASS: GDB observed the dynrec decoder and return from a translated code block.')
PY
else
    status=$?
    tail -20 "$run_dir/run.log" >&2
    echo "FAIL: DOSBox-X exited with status $status; see $run_dir/run.log" >&2
    exit "$status"
fi
