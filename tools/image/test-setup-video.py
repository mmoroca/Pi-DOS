#!/usr/bin/env python3
"""Check real DOS setup BIOS presentation under the packaged ARM64 runtime."""
import argparse
import os
from pathlib import Path
import shutil
import struct
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--sdl-library-path', type=Path, required=True,
                    help='ARM64 SDL2 with dummy video/audio backends')
args = parser.parse_args()
target = ROOT / 'output/buildroot-arm64/target'
base = ROOT / 'output/smoke'
base.mkdir(exist_ok=True)
work = Path(tempfile.mkdtemp(prefix='setup-video.', dir=base))
env = dict(os.environ, SDL_VIDEODRIVER='dummy', SDL_AUDIODRIVER='dummy')
for key in ('PI_DOS_SUPERVISED', 'PI_DOS_SYSTEM_STORAGE', 'PI_DOS_USB_FLOPPY',
            'PI_DOS_USB_CDROM', 'PI_DOS_USB_STORAGE', 'PI_DOS_DIAGNOSTICS', 'LD_PRELOAD'):
    env.pop(key, None)
for name, page, key, state, expected_exit in (
        ('skip', 0, '011bh', 'pending', 0),
        ('page1', 1, '011bh', 'pending', 0),
        ('save', 0, '03c00h', 'pending', 10),
        ('review', 0, '03c00h', 'clock', 0),
        ('setup', 0, '011bh', 'pending', 0)):
    case = work / name
    (case / 'SYSTEM').mkdir(parents=True)
    shutil.copyfile(ROOT / 'br2-external/board/raspberrypi/dos/PIDOSCFG.EXE',
                    case / 'PIDOSCFG.EXE')
    for catalog in ('KEYBOARD.LST', 'COUNTRY.LST', 'TIMEZONE.LST'):
        shutil.copyfile(target / 'pi-dos/drivez/BIN' / catalog, case / catalog)
    config = (target / 'pi-dos/PIDOS.CFG').read_bytes().replace(
        b'first_boot=pending', ('first_boot=' + state).encode())
    (case / 'SYSTEM/PIDOS.CFG').write_bytes(config)
    subprocess.run(['nasm', '-f', 'bin', '-DPAGE=' + str(page), '-DKEY=' + key,
                    '-DFIRSTBOOT=' + ('0' if name == 'setup' else '1'),
                    str(ROOT / 'tools/dos/test-video-guard.asm'), '-o',
                    str(case / 'MODEGUARD.COM')], check=True)
    cmd = [str(ROOT / 'output/tools/qemu-user/usr/bin/qemu-aarch64'),
           '-L', str(target), '-E', 'LD_LIBRARY_PATH=' + str(args.sdl_library_path.resolve()),
           str(target / 'pi-dos/pi-dos'), '-conf', str(ROOT / 'configs/pi-dos.conf'),
           '-defaultmapper', '-nopromptfolder', '-defaultdir', str(case),
           '-fastlaunch', '-nomenu', '-nogui', '-time-limit', '8']
    for setting in ('sdl fullscreen=false', 'sdl videodriver=dummy',
                    'sdl waitonerror=false', 'midi mpu401=none', 'midi mididevice=none'):
        cmd += ['-set', setting]
    for command in ('mount c ' + str(case), 'c:', 'MODEGUARD.COM', 'exit'):
        cmd += ['-c', command]
    with (case / 'run.log').open('w') as log:
        result = subprocess.run(cmd, env=env, stdout=log, stderr=subprocess.STDOUT, timeout=25)
    assert result.returncode == 0, case / 'run.log'
    assert (case / 'MODE.OUT').is_file(), ('Setup did not present a BIOS frame', case)
    resets, mode, active_page, columns, exit_status, rows = struct.unpack(
        '<HBBBBH', (case / 'MODE.OUT').read_bytes())
    assert (resets, mode, active_page, columns, exit_status) == (
        0, 3, page, 80, expected_exit), (name, resets, mode, active_page, columns, exit_status)
    assert rows >= 25, (name, rows)
    expected_state = b'first_boot=clock' if expected_exit == 10 else b'first_boot=done'
    if name == 'setup':
        expected_state = b'first_boot=pending'
        assert (case / 'SYSTEM/PIDOS.CFG').read_bytes() == config
    assert expected_state in (case / 'SYSTEM/PIDOS.CFG').read_bytes()
print('PASS: ARM64 PIDOSCFG and welcome present through BIOS without mode resets; active page, Skip, Save and clock review: ' + str(work))
