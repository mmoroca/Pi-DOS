# The KMS symbols below belong to the pinned native SDL2 build (not stripped).
set pagination off
set confirm off
set breakpoint pending on
handle SIGSEGV nostop noprint pass
python
import gdb, json, os
from pathlib import Path
directory = Path(os.environ['ARMDOS_CAPTURE_DIR'])
stats = {'swap_calls': 0, 'completed_flips': 0, 'flips_after_capture': 0,
         'capture_swap': None}
class Swap(gdb.Breakpoint):
    def stop(self):
        stats['swap_calls'] += 1
        if stats['capture_swap'] is None and (directory/'READY.OK').exists():
            stats['capture_swap'] = stats['swap_calls']
            return True
        return False
class Flip(gdb.Breakpoint):
    def stop(self):
        stats['completed_flips'] += 1
        if stats['capture_swap'] is not None:
            stats['flips_after_capture'] += 1
        return False
Swap('KMSDRM_GLES_SwapWindow', internal=True)
Flip('KMSDRM_FlipHandler', internal=True)
end
run
python
if stats['capture_swap'] is None:
    (directory/'presentation.json').write_text(json.dumps(stats, indent=2)+'\n')
    gdb.execute('bt 24')
    raise gdb.GdbError('DOSBox-X stopped before the graphics capture point')
end
bt 8
print sdl.clip
print *sdl.surface
set $capture_result = (int)armdos_capture_frame((void *)sdl.window)
printf "ARMDOS_GPU_CAPTURE_RESULT=%d\n", $capture_result
continue
python
stats['capture_result'] = int(gdb.parse_and_eval('$capture_result'))
(directory/'presentation.json').write_text(json.dumps(stats, indent=2)+'\n')
print('ARMDOS_PRESENTATION=' + json.dumps(stats))
end
