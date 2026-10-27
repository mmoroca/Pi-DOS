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
#include "reboot_shortcut.h"
int main() {
    SDL_Event event = {};
    event.type = SDL_KEYDOWN; event.key.keysym.scancode = SDL_SCANCODE_DELETE;
    unsetenv("PI_DOS_SUPERVISED");
    assert(!pi_dos_supervisor_stopping());
    assert(!pi_dos_reboot_shortcut(event, true, true));
    setenv("PI_DOS_SUPERVISED", "1", 1);
    assert(!pi_dos_reboot_shortcut(event, true, true)); // desktop parent
    marker = 0;
    assert(!pi_dos_supervisor_stopping());
    parent = 1;
    assert(pi_dos_supervisor_stopping());
    marker = -1;
    assert(!pi_dos_supervisor_stopping());
    assert(!pi_dos_reboot_shortcut(event, false, false));
    assert(!pi_dos_reboot_shortcut(event, true, false));
    assert(!pi_dos_reboot_shortcut(event, false, true));
    event.key.keysym.scancode = SDL_SCANCODE_BACKSPACE;
    assert(!pi_dos_reboot_shortcut(event, true, true));
    event.key.keysym.scancode = SDL_SCANCODE_DELETE; event.type = SDL_KEYUP;
    assert(!pi_dos_reboot_shortcut(event, true, true));
    event.type = SDL_KEYDOWN; event.key.repeat = 1;
    assert(pi_dos_reboot_shortcut(event, true, true) && calls == 0);
    event.key.repeat = 0;
    assert(pi_dos_reboot_shortcut(event, true, true));
    assert(calls == 1 && target == 1 && sent == SIGINT && pi_dos_reboot_requested);
    assert(pi_dos_reboot_shortcut(event, true, true) && calls == 1);
    event.type = SDL_KEYUP;
    assert(pi_dos_reboot_shortcut(event, false, false) && calls == 1);
    pi_dos_reboot_requested = false; event.type = SDL_KEYDOWN; result = -1;
    assert(pi_dos_reboot_shortcut(event, true, true) && !pi_dos_reboot_requested);
    result = 0; event.key.keysym.scancode = SDL_SCANCODE_KP_PERIOD;
    assert(pi_dos_reboot_shortcut(event, true, true) && pi_dos_reboot_requested);
    assert(calls == 3 && logs == 3);
    event.type = SDL_QUIT;
    assert(!pi_dos_reboot_shortcut(event, true, true));
    unsetenv("PI_DOS_SUPERVISED"); event.type = SDL_KEYDOWN;
    marker = 0;
    assert(!pi_dos_supervisor_stopping());
    assert(!pi_dos_reboot_shortcut(event, true, true));
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
print('PASS: supervised reboot signal, ordinary keys, repeats, keypad Delete, failed requests and desktop isolation; no real signals sent.')
