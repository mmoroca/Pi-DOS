#!/usr/bin/env python3
"""Run the ARM64 shell, SDL shortcut dispatch and PNG writer under user-mode QEMU.

Only temporary directories and synthetic SDL events are used; no devices or
real signals are accessed. Requires an ARM64 SDL2 with dummy video support.
"""
import argparse
import os
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--sdl-library-path', type=Path, required=True)
parser.add_argument('--runtime', type=Path)
args = parser.parse_args()
runtime = next((ROOT / 'output/buildroot-arm64/build').glob('pi-dos-runtime-*'))
binary = (args.runtime or runtime / 'src/dosbox-x').resolve()
target = ROOT / 'output/buildroot-arm64/target'
qemu = ROOT / 'output/tools/qemu-user/usr/bin/qemu-aarch64'
base = ROOT / 'output/smoke'
base.mkdir(exist_ok=True)
work = Path(tempfile.mkdtemp(prefix='capture-workflow.', dir=base))
env = dict(os.environ, SDL_VIDEODRIVER='dummy', SDL_AUDIODRIVER='dummy')
for name in list(env):
    if name.startswith('PI_DOS_') or name == 'LD_PRELOAD':
        env.pop(name)


def run(case, commands, settings=(), preload=None, seconds=8):
    command = [str(qemu), '-L', str(target), '-E',
               'LD_LIBRARY_PATH=' + str(args.sdl_library_path.resolve())]
    if preload:
        command += ['-E', 'LD_PRELOAD=' + str(preload)]
    command += [str(binary), '-conf', str(ROOT / 'configs/pi-dos.conf'),
                '-defaultmapper', '-nopromptfolder', '-defaultdir', str(case),
                '-fastlaunch', '-nomenu', '-nogui', '-time-limit', str(seconds)]
    for setting in ('sdl fullscreen=false', 'sdl videodriver=dummy',
                    'sdl waitonerror=false', 'midi mpu401=none', 'midi mididevice=none',
                    *settings):
        command += ['-set', setting]
    for line in ('mount c ' + str(case), 'c:', *commands):
        command += ['-c', line]
    with (case / 'run.log').open('w') as log:
        result = subprocess.run(command, env=env, stdout=log,
                                stderr=subprocess.STDOUT, timeout=45)
    assert result.returncode == 0, case / 'run.log'


for lfn in ('true', 'false'):
    case = work / ('lfn-' + lfn)
    case.mkdir()
    batch = ['@ECHO OFF']
    for index, command in enumerate(('DEL. /Q', 'DEL *.* /Q')):
        for names in ('SHORT', 'LONG'):
            directory = case / f'{names}{index}'
            directory.mkdir()
            for number in range(24):
                filename = f'F{number}.PNG' if names == 'SHORT' else f'command_{number:03}.png'
                (directory / filename).write_bytes(b'capture')
            (directory / 'KEEP.TXT').write_bytes(b'read-only')
            (directory / 'KEEP.TXT').chmod(0o444)
            batch += ['CD \\' + directory.name, command]
    batch += ['EXIT']
    (case / 'RUN.BAT').write_bytes(('\r\n'.join(batch) + '\r\n').encode())
    run(case, ['RUN.BAT'], ['dos lfn=' + lfn])
    for directory in case.iterdir():
        if directory.is_dir():
            remaining = sorted(p.name for p in directory.iterdir())
            assert remaining == ['KEEP.TXT'], (lfn, directory, remaining, case / 'run.log')

# Interpose SDL_PollEvent to feed the production event loop. Supervision is
# enabled only while returning synthetic key events, then removed before the
# renderer runs. This exercises the appliance shortcut while leaving captures
# and the DOS drive safely in the temporary standalone directories.
source = work / 'events.c'
source.write_text(r'''
#define _GNU_SOURCE
#include <SDL.h>
#include <dlfcn.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
pid_t getppid(void) { return 1; }
int kill(pid_t pid, int signal) { (void)pid; (void)signal; errno=EPERM; return -1; }
struct Event { unsigned when; SDL_Scancode code; Uint32 type; Uint16 mods; Uint8 repeat; };
#define CA (KMOD_CTRL | KMOD_ALT)
#define DOWN(t,k,m) {t,SDL_SCANCODE_##k,SDL_KEYDOWN,m,0}
#define UP(t,k,m) {t,SDL_SCANCODE_##k,SDL_KEYUP,m,0}
static const struct Event events[] = {
    DOWN(1800,PRINTSCREEN,CA), UP(1820,PRINTSCREEN,0),
    DOWN(2200,LCTRL,KMOD_CTRL), DOWN(2220,LALT,CA),
    DOWN(2240,PRINTSCREEN,CA), UP(2260,PRINTSCREEN,CA), UP(2280,LALT,KMOD_CTRL), UP(2300,LCTRL,0),
    DOWN(2600,LCTRL,KMOD_CTRL), DOWN(2620,LALT,CA),
    DOWN(2640,PRINTSCREEN,CA), /* A release consumed outside the main loop. */
    UP(2680,LALT,KMOD_CTRL), UP(2700,LCTRL,0),
    DOWN(3000,LCTRL,KMOD_CTRL), DOWN(3020,LALT,CA), DOWN(3040,PRINTSCREEN,CA),
    {3060,SDL_SCANCODE_PRINTSCREEN,SDL_KEYDOWN,CA,1},
    UP(3080,PRINTSCREEN,CA), UP(3100,LALT,KMOD_CTRL), UP(3120,LCTRL,0),
    DOWN(3500,SYSREQ,CA), UP(3520,SYSREQ,0)
};
int SDL_PollEvent(SDL_Event *event) {
    static int (*real_poll)(SDL_Event *);
    static unsigned index;
    static unsigned long start;
    struct timespec now;
    unsetenv("PI_DOS_SUPERVISED");
    if (!real_poll) real_poll=dlsym(RTLD_NEXT,"SDL_PollEvent");
    int result=real_poll(event);
    if (result || !event) return result;
    clock_gettime(CLOCK_MONOTONIC,&now);
    unsigned long ms=(unsigned long)now.tv_sec*1000 + now.tv_nsec/1000000;
    if (!start) start=ms;
    if (index >= sizeof(events)/sizeof(events[0]) || ms-start < events[index].when) return 0;
    const struct Event *next=&events[index++];
    SDL_zero(*event);
    event->type=next->type;event->key.type=next->type;
    event->key.state=next->type==SDL_KEYDOWN ? SDL_PRESSED : SDL_RELEASED;
    event->key.repeat=next->repeat;event->key.keysym.scancode=next->code;
    event->key.keysym.sym=SDL_GetKeyFromScancode(next->code);
    event->key.keysym.mod=next->mods;
    setenv("PI_DOS_SUPERVISED","1",1);
    fprintf(stderr,"TEST: SDL key %u type %u repeat %u\n",next->code,next->type,next->repeat);
    return 1;
}
''')
library = work / 'events.so'
host = ROOT / 'output/buildroot-arm64/host'
subprocess.run([str(host / 'bin/aarch64-linux-gcc'), '-shared', '-fPIC', '-Wall', '-Wextra', '-Werror',
                '-I' + str(host / 'aarch64-buildroot-linux-gnu/sysroot/usr/include/SDL2'),
                str(source), '-ldl', '-o', str(library)], check=True)
case = work / 'shots'
case.mkdir()
captures = case / 'CAPTURES'
captures.mkdir()
for program in ('COMMAND', 'DOSBOX'):
    (captures / (program + '_007.PNG')).write_bytes(b'previous capture')
run(case, ['CLS'], ['dosbox captures=' + str(captures)], preload=library, seconds=6)
shots = [path for path in captures.iterdir() if path.read_bytes().startswith(b'\x89PNG\r\n\x1a\n')]
assert len(shots) == 5, (len(shots), [p.name for p in captures.iterdir()], case / 'run.log')
assert all(p.read_bytes()[-8:-4] == b'IEND' for p in shots), 'Incomplete PNG output'
assert all(int(p.stem.rsplit('_', 1)[1]) >= 8 for p in shots), [p.name for p in shots]
for program in ('COMMAND', 'DOSBOX'):
    assert (captures / (program + '_007.PNG')).read_bytes() == b'previous capture'
print('PASS: ARM64 DEL./DEL *.* with short/long aliases and SFN/LFN; five SDL shortcut PNGs, missed releases, repeat suppression and uppercase numbering: ' + str(work))
