#!/usr/bin/env python3
"""Check writable CONFIG.SYS import and a real DOS driver before AUTOEXEC."""
import os
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]
target = ROOT / 'output/buildroot-arm64/target'
qemu = ROOT / 'output/tools/qemu-user/usr/bin/qemu-aarch64'
sdl = ROOT / 'output/investigation/wolf3d/sdl-build/build/.libs'

def main():
    work = Path(tempfile.mkdtemp(prefix='config-sys.', dir=ROOT / 'output/smoke'))
    wrapper = work / 'import.c'
    wrapper.write_text('''
#include "config.h"
int main(int argc, char **argv) {
    return argc != 3 || !pi_dos_config_sys(argv[1], argv[2]);
}
''')
    importer = work / 'import'
    subprocess.run(['cc', '-std=c11', '-Wall', '-Wextra', '-Werror',
                    '-I' + str(ROOT / 'src/init'), '-I' + str(ROOT / 'src/common'), str(wrapper),
                    str(ROOT / 'src/init/config.c'), '-o', str(importer)], check=True)
    config = work / 'CONFIG.SYS'
    generated = work / 'dos.conf'

    def convert(data):
        config.write_bytes(data)
        result = subprocess.run([str(importer), str(config), str(generated)],
                                capture_output=True, text=True, check=True)
        assert config.read_bytes() == data
        return generated.read_text(), result.stderr

    output, warnings = convert(
        b'REM\tinstaller settings\r\nFILES = 40\r\nDOS=HIGH,UMB\r\n'
        b'DEVICE=C:\\WINDOWS\\HIMEM.SYS\r\nDEVICEHIGH="C:\\DOS\\EMM386.EXE" NOEMS\r\n'
        b'DEVICE=C:\\CFGTEST.SYS\r\nSET CFG_STAGE=CONFIG\r\n'
        b'BUFFERS=30\r\nSHELL=C:\\COMMAND.COM /P\r\n\x1aFILES=1\r\n')
    assert output == ('[config]\nfiles=40\ndos=HIGH,UMB\n'
                      'device=C:\\CFGTEST.SYS\nset cfg_stage=CONFIG\n')
    assert 'himem' not in output.lower() and 'emm386' not in output.lower()
    assert 'internal memory manager' in warnings and 'buffers' in warnings and 'shell' in warnings
    output, warnings = convert(b'FILES=40\r\n[menu]\r\nDEVICE=C:\\OTHER.SYS\r\n[autoexec]\r\nEXIT\r\n')
    assert output == '[config]\nfiles=40\n' and 'boot-menu' in warnings
    config.unlink()
    subprocess.run([str(importer), str(config), str(generated)], check=True)
    assert generated.read_text() == '[config]\n'

    # QEMU redirects /dosdata to this fixture for the early registration check.
    # Actual file I/O below uses a normal temporary mount: production's sharing
    # checks correctly reject QEMU aliases whose /proc/self/fd paths differ.
    sysroot = work / 'sysroot'
    sysroot.mkdir()
    for name in ('lib', 'usr'):
        (sysroot / name).symlink_to(target / name, target_is_directory=True)
    data = sysroot / 'dosdata'
    data.mkdir()
    driver = work / 'driver.asm'
    driver.write_text('''bits 16
org 0
dd 0xffffffff
dw 0x8000
dw strategy, interrupt
db 'CFGTEST$'
request: dd 0
strategy:
    mov [cs:request], bx
    mov [cs:request+2], es
    retf
interrupt:
    push bx
    push es
    les bx, [cs:request]
    mov word [es:bx+3], 0x0100
    cmp byte [es:bx+2], 0
    jne done
    mov word [es:bx+14], resident_end
    mov [es:bx+16], cs
done:
    pop es
    pop bx
    retf
resident_end:
''')
    check = work / 'check.asm'
    check.write_text('''bits 16
org 100h
mov dx, device
mov ax, 3d00h
int 21h
jc failure
mov bx, ax
mov ah, 3eh
int 21h
mov ax, 3305h
int 21h
cmp dl, 3
jne failure
mov dx, success
mov ah, 9
int 21h
mov ax, 4c00h
int 21h
failure:
mov ax, 4c01h
int 21h
device: db 'CFGTEST$', 0
success: db 'CONFIG_DRIVER_OK', 13, 10, '$'
''')
    for source, name in ((driver, 'CFGTEST.SYS'), (check, 'CHECK.COM')):
        subprocess.run(['nasm', '-f', 'bin', str(source), '-o', str(data / name)], check=True)
    config = data / 'CONFIG.SYS'
    (data / 'AUTOEXEC.BAT').write_text(
        '@ECHO OFF\nCHECK.COM >CHECK.OUT\nECHO %CFG_STAGE% >STAGE.OUT\n'
        'Z:\\SYSTEM\\CONFIG.COM -get config files >FILES.OUT\nEXIT\n')
    boot = work / 'boot.conf'
    # Use precisely the early C: registration emitted by PID 1.
    boot.write_text('[devices]\nRUN=Z:\\SYSTEM\\PISYSTEM.COM\n[autoexec]\nEXIT\n')
    env = dict(os.environ, SDL_VIDEODRIVER='dummy', SDL_AUDIODRIVER='dummy',
               PI_DOS_SUPERVISED='1', PI_DOS_SYSTEM_STORAGE='rw')
    for name in ('PI_DOS_USB_FLOPPY', 'PI_DOS_USB_CDROM', 'PI_DOS_USB_STORAGE'):
        env.pop(name, None)
    def run(prefix):
        libraries = ':'.join(str(path) for path in (sdl, target / 'lib', target / 'usr/lib'))
        command = [str(qemu), '-L', str(prefix), '-E', 'LD_LIBRARY_PATH=' + libraries,
                   str(target / 'pi-dos/pi-dos'), '-defaultconf', '-defaultmapper',
                   '-nopromptfolder', '-defaultdir', str(work), '-fastlaunch', '-nomenu',
                   '-nogui', '-time-limit', '3', '-conf', str(boot), '-conf', str(generated)]
        for setting in ('sdl fullscreen=false', 'sdl output=surface', 'sdl videodriver=dummy',
                        'sdl waitonerror=false', 'cpu core=normal', 'cpu cycles=fixed 20000',
                        'midi mpu401=none'):
            command += ['-set', setting]
        result = subprocess.run(command, env=env, capture_output=True, timeout=30)
        assert result.returncode == 0, result.stdout + result.stderr
        return result

    generated.write_text('[config]\n')
    result = run(sysroot)
    assert b'Pi-DOS: system C: registered (rw)' in result.stderr
    (work / 'early-c.log').write_bytes(result.stdout + result.stderr)
    env.pop('PI_DOS_SUPERVISED')
    env.pop('PI_DOS_SYSTEM_STORAGE')
    boot.write_text(f'[devices]\nRUN=MOUNT C "{data}"\n'
                    '[autoexec]\n@ECHO OFF\nC:\nCALL C:\\AUTOEXEC.BAT\n')
    for stage, files in (('CONFIG', 40), ('INSTALLER_EDIT', 60)):
        contents = f'FILES={files}\r\nDEVICE=C:\\CFGTEST.SYS\r\nSET CFG_STAGE={stage}\r\n'.encode()
        convert(contents)
        for name in ('CHECK.OUT', 'STAGE.OUT', 'FILES.OUT'):
            (data / name).unlink(missing_ok=True)
        result = run(target)
        (work / (stage + '.log')).write_bytes(result.stdout + result.stderr)
        assert (data / 'CHECK.OUT').exists(), result.stdout + result.stderr
        assert (data / 'CHECK.OUT').read_bytes().strip() == b'CONFIG_DRIVER_OK', result.stdout + result.stderr
        assert (data / 'STAGE.OUT').read_bytes().strip() == stage.encode()
        assert (data / 'FILES.OUT').read_bytes().strip() == str(files).encode()
        assert config.read_bytes() == contents

    print('PASS: CONFIG.SYS preservation, parsing, early C:, real DOS driver before AUTOEXEC, and installer edits after restart.')
    print('Evidence: ' + str(work))


if __name__ == '__main__':
    main()
