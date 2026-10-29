#!/usr/bin/env python3
"""Exercise the production controller bridge with SDL virtual devices, no real input."""
from pathlib import Path
import ctypes
import subprocess
import tempfile
ROOT = Path(__file__).resolve().parents[2]
with tempfile.TemporaryDirectory(prefix='pidos-input.') as directory:
    work = Path(directory)
    library = work/'input.so'
    subprocess.run(['cc', '-std=c11', '-Wall', '-Wextra', '-Werror', '-shared', '-fPIC',
                    '-I'+str(ROOT/'src/common'), str(ROOT/'src/init/config.c'), '-o', str(library)], check=True)
    config = ctypes.CDLL(str(library))
    config.pi_dos_input_prepare.argtypes = [ctypes.c_char_p]
    libc = ctypes.CDLL(None)
    libc.getenv.argtypes = [ctypes.c_char_p]
    libc.getenv.restype = ctypes.c_char_p
    config.pi_dos_input_prepare(None)
    assert libc.getenv(b'PI_DOS_SHORTCUT_REBOOT') == b'1'
    assert libc.getenv(b'PI_DOS_SHORTCUT_POWEROFF') == b'0'
    assert libc.getenv(b'PI_DOS_SHORTCUT_SCREENSHOT') == b'12'
    assert libc.getenv(b'PI_DOS_SHORTCUT_NEXT_CD') == b'5'
    assert libc.getenv(b'PI_DOS_SHORTCUT_NEXT_FLOPPY') == b'6'
    path = work/'PIDOS.CFG'
    path.write_text('[pidos]\ncontroller_layout=RAW\ncontroller_direction=dpad\n'
                    'shortcut_reboot=none\nshortcut_poweroff=Ctrl+Alt+F2\nshortcut_pause=ctrl+alt+f2\n'
                    'shortcut_screenshot=ctrl+alt+printscreen\nshortcut_next_cd=Ctrl+Alt+PrintScreen\n'
                    'shortcut_next_floppy=invalid\n')
    config.pi_dos_input_prepare(str(path).encode())
    assert libc.getenv(b'PI_DOS_CONTROLLER_LAYOUT') == b'raw'
    assert libc.getenv(b'PI_DOS_SHORTCUT_REBOOT') == b'0'
    assert libc.getenv(b'PI_DOS_SHORTCUT_POWEROFF') == b'3'
    assert libc.getenv(b'PI_DOS_SHORTCUT_PAUSE') == b'0'  # duplicate disabled
    assert libc.getenv(b'PI_DOS_SHORTCUT_SCREENSHOT') == b'12'
    assert libc.getenv(b'PI_DOS_SHORTCUT_NEXT_CD') == b'0'  # duplicate disabled
    assert libc.getenv(b'PI_DOS_SHORTCUT_NEXT_FLOPPY') == b'6'  # invalid falls back
    source = work/'controllers.cpp'
    source.write_text(r'''
#include <assert.h>
#include "controllers.h"
static void pump() { SDL_JoystickUpdate(); pidos_controllers.changed(); }
int main() {
    unsetenv("PI_DOS_CONTROLLER_LAYOUT"); unsetenv("PI_DOS_CONTROLLER_DIRECTION");
    unsetenv("PI_DOS_CONTROLLER_SWAP_BUTTONS");
    SDL_SetHint(SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS,"1");
    assert(SDL_Init(SDL_INIT_JOYSTICK)==0);
    setenv("PI_DOS_SUPERVISED","1",1);
    assert(pidos_native_controllers());
    assert(!pidos_controllers.sample(0).connected); // no controller at boot
    int first=SDL_JoystickAttachVirtual(SDL_JOYSTICK_TYPE_UNKNOWN,4,6,1);
    assert(first>=0);
    SDL_Joystick *one=SDL_JoystickOpen(first); assert(one);
    SDL_JoystickSetVirtualAxis(one,0,16000);
    SDL_JoystickSetVirtualButton(one,0,1);
    pump();
    auto state=pidos_controllers.sample(0);
    assert(state.connected && state.axes[0]==16000 && state.buttons[0]);
    int second=SDL_JoystickAttachVirtual(SDL_JOYSTICK_TYPE_UNKNOWN,4,6,1);
    assert(second>=0);
    SDL_Joystick *two=SDL_JoystickOpen(second); assert(two);
    SDL_JoystickSetVirtualAxis(two,0,-22000); pump();
    assert(pidos_controllers.sample(1).axes[0]==-22000);
    assert(SDL_JoystickDetachVirtual(first)==0); pump();
    state=pidos_controllers.sample(0);
    assert(!state.connected && !state.buttons[0] && state.axes[0]==0);
    assert(pidos_controllers.sample(1).axes[0]==-22000); // slot does not shift
    int replacement=SDL_JoystickAttachVirtual(SDL_JOYSTICK_TYPE_UNKNOWN,4,6,1);
    SDL_Joystick *three=SDL_JoystickOpen(replacement); assert(three);
    SDL_JoystickSetVirtualButton(three,0,1);
    SDL_JoystickSetVirtualHat(three,0,SDL_HAT_LEFTUP); pump();
    state=pidos_controllers.sample(0);
    assert(state.connected && state.axes[0]==-32767 && state.axes[1]==-32767);
    setenv("PI_DOS_CONTROLLER_SWAP_BUTTONS","true",1);
    state=pidos_controllers.sample(0); assert(!state.buttons[0] && state.buttons[1]);
    setenv("PI_DOS_CONTROLLER_DIRECTION","stick",1);
    assert(pidos_controllers.sample(0).axes[0]==0);
    setenv("PI_DOS_CONTROLLER_DIRECTION","dpad",1);
    assert(pidos_controllers.sample(1).axes[0]==0);
    pidos_controllers.reset();
    SDL_JoystickClose(one); SDL_JoystickClose(two); SDL_JoystickClose(three);
    // A known gamepad's A button can live at a different raw button index.
    int mapped=SDL_JoystickAttachVirtual(SDL_JOYSTICK_TYPE_UNKNOWN,4,6,1);
    SDL_Joystick *pad=SDL_JoystickOpen(mapped); assert(pad);
    char guid[33]; SDL_JoystickGetGUIDString(SDL_JoystickGetGUID(pad),guid,sizeof(guid));
    char mapping[512];
    SDL_snprintf(mapping,sizeof(mapping),"%s,Pi-DOS test,a:b3,b:b2,x:b1,y:b0,leftx:a2,lefty:a3,rightx:a0,righty:a1,dpup:h0.1,dpright:h0.2,dpdown:h0.4,dpleft:h0.8,",guid);
    assert(SDL_GameControllerAddMapping(mapping)>=0 && SDL_IsGameController(mapped));
    SDL_JoystickSetVirtualAxis(pad,2,19000); SDL_JoystickSetVirtualButton(pad,3,1);
    unsetenv("PI_DOS_CONTROLLER_DIRECTION"); unsetenv("PI_DOS_CONTROLLER_SWAP_BUTTONS");
    // Existing virtual devices occupy the first two slots; remove them first.
    for(int i=SDL_NumJoysticks()-1;i>=0;--i)
        if(SDL_JoystickGetDeviceInstanceID(i)!=SDL_JoystickInstanceID(pad)) SDL_JoystickDetachVirtual(i);
    pump(); state=pidos_controllers.sample(0);
    assert(state.axes[0]==19000 && state.buttons[0]); // normalized, not raw
    pidos_controllers.reset(); SDL_JoystickClose(pad); SDL_Quit();
}
''')
    headers = ROOT/'output/buildroot-arm64/host/aarch64-buildroot-linux-gnu/sysroot/usr/include/SDL2'
    binary = work/'controllers'
    subprocess.run(['c++', '-std=c++14', '-Wall', '-Wextra', '-Werror', '-I'+str(headers),
                    '-I'+str(ROOT/'src/runtime'), str(source), '-l:libSDL2-2.0.so.0', '-o', str(binary)], check=True)
    subprocess.run([str(binary)], check=True)
print('PASS: late connection, stable slots, unplug cleanup, replacement, D-pad, button swap, normalization and config handoff.')
