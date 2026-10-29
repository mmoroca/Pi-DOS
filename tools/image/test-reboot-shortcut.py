#!/usr/bin/env python3
"""Exercise the production shortcut helper without sending real signals."""
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]
with tempfile.TemporaryDirectory(prefix='pi-dos-reboot.') as temporary:
    work = Path(temporary)
    source = work / 'shortcut.cpp'
    source.write_text(r'''
#include <SDL.h>
#include <assert.h>
#include <errno.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
static pid_t parent = 2, target = -1;
static int calls = 0, sent = 0, result = 0, logs = 0, marker = -1;
static pid_t fake_getppid() { return parent; }
static int fake_kill(pid_t pid, int sig) { ++calls; target = pid; sent = sig; errno = EPERM; return result; }
static int fake_access(const char *path, int mode) { assert(!strcmp(path, "/run/pi-dos-stop") && mode == F_OK); return marker; }
#define getppid fake_getppid
#define kill fake_kill
#define access fake_access
#define LOG_MSG(...) (++logs)
#define LINUX 1
#define C_SDL2 1
#define C_SSHOT 1
#include "reboot_shortcut.h"
static int pauses=0, screenshots=0, cds=0, floppies=0;
void PauseDOSBox(bool pressed) { assert(pressed); ++pauses; }
void CAPTURE_ScreenShotEvent(bool pressed) { assert(pressed); ++screenshots; }
void PiDosNextImage(bool cdrom) { if(cdrom) ++cds; else ++floppies; }
int main() {
    SDL_Event event = {};
    event.type=SDL_KEYDOWN; event.key.keysym.scancode=SDL_SCANCODE_DELETE;
    unsetenv("PI_DOS_SUPERVISED");
    assert(!pi_dos_supervisor_stopping());
    assert(!pi_dos_reboot_shortcut(event,true,true));
    setenv("PI_DOS_SUPERVISED","1",1);
    assert(!pi_dos_reboot_shortcut(event,true,true));
    parent=1; marker=0;
    assert(pi_dos_supervisor_stopping()); marker=-1;
    assert(!pi_dos_supervisor_stopping());
    assert(!pi_dos_reboot_shortcut(event,false,true));
    event.key.keysym.mod=KMOD_SHIFT;
    assert(!pi_dos_reboot_shortcut(event,true,true));
    event.key.keysym.mod=KMOD_NONE;
    assert(pi_dos_reboot_shortcut(event,true,true));
    assert(calls==1 && target==1 && sent==SIGINT && pi_dos_reboot_requested);
    event.key.repeat=1;
    assert(pi_dos_reboot_shortcut(event,true,true) && calls==1);
    event.type=SDL_KEYUP; event.key.repeat=0;
    assert(pi_dos_reboot_shortcut(event,false,false)); // modifiers released first
    pi_dos_reboot_requested=false;
    event.type=SDL_KEYDOWN; result=-1;
    assert(pi_dos_reboot_shortcut(event,true,true) && !pi_dos_reboot_requested);
    event.type=SDL_KEYUP; assert(pi_dos_reboot_shortcut(event,false,false));
    result=0; event.type=SDL_KEYDOWN; event.key.keysym.scancode=SDL_SCANCODE_KP_PERIOD;
    assert(pi_dos_reboot_shortcut(event,true,true) && pi_dos_reboot_requested);
    event.type=SDL_KEYUP; assert(pi_dos_reboot_shortcut(event,false,false));
    pi_dos_reboot_requested=false;
    setenv("PI_DOS_SHORTCUT_REBOOT","0",1);
    event.type=SDL_KEYDOWN; event.key.keysym.scancode=SDL_SCANCODE_DELETE;
    assert(!pi_dos_reboot_shortcut(event,true,true)); // disabled CAD goes to DOS
    setenv("PI_DOS_SHORTCUT_POWEROFF","3",1); // F2
    event.key.keysym.scancode=SDL_SCANCODE_F2;
    assert(pi_dos_reboot_shortcut(event,true,true) && sent==SIGTERM);
    event.type=SDL_KEYUP; assert(pi_dos_reboot_shortcut(event,false,false));
    pi_dos_reboot_requested=false;
    setenv("PI_DOS_SHORTCUT_PAUSE","4",1); // F3
    event.type=SDL_KEYDOWN; event.key.keysym.scancode=SDL_SCANCODE_F3;
    assert(pi_dos_reboot_shortcut(event,true,true) && pauses==0);
    event.key.repeat=1;
    assert(pi_dos_reboot_shortcut(event,true,true) && pauses==0);
    event.type=SDL_KEYUP; event.key.repeat=0;
    assert(pi_dos_reboot_shortcut(event,false,false) && pauses==1);
    assert(!pi_dos_reboot_shortcut(event,false,false));
    event.type=SDL_QUIT;
    assert(!pi_dos_reboot_shortcut(event,true,true));
    event.type=SDL_KEYDOWN; event.key.keysym.scancode=SDL_SCANCODE_PRINTSCREEN;
    assert(pi_dos_reboot_shortcut(event,true,true) && screenshots==1);
    event.key.repeat=1;
    assert(pi_dos_reboot_shortcut(event,true,true) && screenshots==1);
    event.type=SDL_KEYUP; event.key.repeat=0;
    assert(pi_dos_reboot_shortcut(event,false,false));
    event.type=SDL_KEYDOWN; event.key.keysym.scancode=SDL_SCANCODE_SYSREQ;
    assert(pi_dos_reboot_shortcut(event,true,true) && screenshots==2);
    event.type=SDL_KEYUP;
    assert(pi_dos_reboot_shortcut(event,false,false));
    event.type=SDL_KEYDOWN; event.key.keysym.scancode=SDL_SCANCODE_F4;
    assert(pi_dos_reboot_shortcut(event,true,true) && cds==1 && floppies==0);
    event.type=SDL_KEYUP;
    assert(pi_dos_reboot_shortcut(event,false,false));
    event.type=SDL_KEYDOWN; event.key.keysym.scancode=SDL_SCANCODE_F5;
    assert(pi_dos_reboot_shortcut(event,true,true) && floppies==1);
    event.type=SDL_KEYUP;
    assert(pi_dos_reboot_shortcut(event,false,false));
    setenv("PI_DOS_SHORTCUT_NEXT_CD","0",1);
    setenv("PI_DOS_SHORTCUT_SCREENSHOT","9",1); // F8
    event.type=SDL_KEYDOWN; event.key.keysym.scancode=SDL_SCANCODE_F4;
    assert(!pi_dos_reboot_shortcut(event,true,true));
    event.key.keysym.scancode=SDL_SCANCODE_PRINTSCREEN;
    assert(!pi_dos_reboot_shortcut(event,true,true));
    event.key.keysym.scancode=SDL_SCANCODE_F8;
    assert(pi_dos_reboot_shortcut(event,true,true) && screenshots==3);
    event.type=SDL_KEYUP;
    assert(pi_dos_reboot_shortcut(event,false,false));
    parent=2; event.type=SDL_KEYDOWN;
    assert(!pi_dos_reboot_shortcut(event,true,true));
    assert(calls==4);
}
''')
    headers = [ROOT/'output/host-sdl2/include/SDL2',
               ROOT/'output/buildroot-arm64/host/aarch64-buildroot-linux-gnu/sysroot/usr/include/SDL2']
    local = next((path for path in headers if (path/'SDL.h').is_file()), None)
    cflags = ['-I'+str(local)] if local else subprocess.check_output(
        ['pkg-config', '--cflags', 'sdl2'], text=True).split()
    binary = work / 'shortcut'
    subprocess.run(['c++', '-std=c++14', '-Wall', '-Wextra', '-Werror', *cflags,
                    '-I'+str(ROOT/'src/common'), str(source), '-o', str(binary)], check=True)
    subprocess.run([str(binary)], check=True)
print('PASS: configurable system/capture/image shortcuts, repeats, release handling, failed requests and desktop isolation; no real signals sent.')
