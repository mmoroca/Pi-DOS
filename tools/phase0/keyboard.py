#!/usr/bin/env python3
"""SSH-only Linux uinput -> SDL2 KMS/evdev -> DOS keyboard diagnostic."""
import fcntl
import json
import os
from pathlib import Path
import signal
import struct
import subprocess
import sys
import tempfile
import time

from graphics import ROOT, drm_owners


def event_owners(node):
    owners = []
    for process in Path('/proc').iterdir():
        if not process.name.isdigit():
            continue
        try:
            for fd in (process / 'fd').iterdir():
                try:
                    if os.readlink(fd) == str(node):
                        owners.append({'pid': int(process.name),
                                       'name': (process / 'comm').read_text().strip()})
                        break
                except OSError:
                    pass
        except OSError:
            pass
    return owners


def wait_for(predicate, process=None, seconds=15):
    deadline = time.monotonic() + seconds
    while time.monotonic() < deadline:
        value = predicate()
        if value:
            return value
        if process is not None and process.poll() is not None:
            raise RuntimeError(f'DOSBox-X exited early ({process.returncode})')
        time.sleep(0.05)
    raise RuntimeError('Timed out waiting for virtual keyboard or DOS readiness')


def main():
    if os.geteuid() != 0:
        raise RuntimeError('This diagnostic requires root for uinput and KMS')
    if drm_owners():
        raise RuntimeError('DRM devices are in use; leaving services running')
    outputs = [p.parent for p in Path('/sys/class/drm').glob('card*-HDMI-*/status')
               if p.read_text().strip() == 'connected']
    if len(outputs) != 1:
        raise RuntimeError('This diagnostic requires exactly one connected HDMI output')
    device = outputs[0].name.split('-')[0][4:]
    modes = (outputs[0] / 'modes').read_text().splitlines()
    if not modes:
        raise RuntimeError('Connected display has no advertised modes')
    binary = ROOT / 'output/build/dosbox-x-sdl2/src/dosbox-x'
    if not binary.is_file():
        raise RuntimeError('Build DOSBox-X first with make phase0')
    (ROOT / 'output/smoke').mkdir(parents=True, exist_ok=True)
    run = Path(tempfile.mkdtemp(prefix='keyboard.', dir=ROOT / 'output/smoke'))
    print('Run directory:', run, flush=True)
    subprocess.run(['nasm', '-f', 'bin', str(ROOT / 'tests/dos/keyboard.asm'),
                    '-o', str(run / 'KEYBOARD.COM')], check=True)
    name = f'Pi-DOS diagnostic keyboard {os.getpid()}'
    node = None
    process = None
    created = False
    fd = os.open('/dev/uinput', os.O_WRONLY | os.O_NONBLOCK)
    try:
        # Linux uinput.h: UI_SET_EVBIT, UI_SET_KEYBIT, UI_DEV_SETUP/CREATE.
        # Native 64-bit Linux input_event uses two longs followed by HH and int.
        if struct.calcsize('@llHHi') != 24:
            raise RuntimeError('This diagnostic requires a 64-bit Linux host')
        fcntl.ioctl(fd, 0x40045564, 1)  # EV_KEY
        for key in range(1, 84):
            fcntl.ioctl(fd, 0x40045565, key)
        fcntl.ioctl(fd, 0x40045565, 105)  # KEY_LEFT
        setup = struct.pack('@HHHH80sI', 3, 0x1209, 0x0001, 1, name.encode(), 0)
        fcntl.ioctl(fd, 0x40005503 | (len(setup) << 16), setup)
        fcntl.ioctl(fd, 0x5501)
        created = True

        def find_node():
            for entry in Path('/sys/class/input').glob('event*'):
                try:
                    if (entry / 'device/name').read_text().strip() == name:
                        candidate = Path('/dev/input') / entry.name
                        if candidate.exists():
                            return candidate
                except OSError:
                    pass
            return None

        node = wait_for(find_node)
        # The development Pi uses udev; finish its new-device classification
        # before SDL's initial scan. No service is installed or reconfigured.
        subprocess.run(['udevadm', 'settle', '--timeout=3'], check=True, timeout=5)
        env = dict(os.environ)
        env.pop('LD_PRELOAD', None)
        env.update(SDL_VIDEODRIVER='kmsdrm', SDL_AUDIODRIVER='dummy',
                   SDL_KMSDRM_DEVICE_INDEX=device, SDL_RENDER_DRIVER='opengles2',
                   SDL_FRAMEBUFFER_ACCELERATION='opengles2')
        command = [str(binary), '-defaultconf', '-defaultmapper', '-nopromptfolder',
                   '-defaultdir', str(run), '-fastlaunch', '-nomenu', '-fullscreen',
                   '-set', 'sdl output=surface', '-set', f'sdl fullresolution={modes[0]}',
                   '-set', 'sdl videodriver=kmsdrm', '-set', 'sdl waitonerror=false',
                   '-set', 'cpu core=normal', '-set', 'dos keyboardlayout=us',
                   '-c', f'mount c "{run}"', '-c', 'c:',
                   '-c', 'keyboard.com >KEYBOARD.OUT', '-c', 'exit']
        with (run / 'run.log').open('w') as log:
            process = subprocess.Popen(command, cwd=ROOT, env=env, stdout=log,
                                       stderr=subprocess.STDOUT, start_new_session=True)
            wait_for(lambda: (run / 'READY.OK').exists(), process)
            owners = event_owners(node)
            if not any(owner['pid'] == process.pid for owner in owners):
                raise RuntimeError(f'DOSBox-X did not open {node}: {owners}')
            unexpected = [owner for owner in owners if owner['pid'] != process.pid
                          and owner['name'] != 'systemd-logind']
            if unexpected:
                raise RuntimeError(f'Other applications opened the virtual keyboard: {unexpected}')

            def send(key, down):
                if down and process.poll() is not None:
                    raise RuntimeError('DOSBox-X exited while sending keys')
                os.write(fd, struct.pack('@llHHi', 0, 0, 1, key, down)
                         + struct.pack('@llHHi', 0, 0, 0, 0, 0))
                time.sleep(0.07)

            def press(key, shift=False):
                if shift:
                    send(42, 1)
                send(key, 1)
                send(key, 0)
                if shift:
                    send(42, 0)

            # BIOS: a b A a 1 Backspace Enter F1 Left. DOS: x Y 3.
            sequence = [(30, False), (48, False), (30, True), (30, False),
                        (2, False), (14, False), (28, False), (59, False),
                        (105, False), (45, False), (21, True), (4, False)]
            for key, shift in sequence:
                press(key, shift)
            process.wait(timeout=15)
        if process.returncode:
            raise RuntimeError(f'DOSBox-X exited {process.returncode}; see {run}/run.log')
        expected_words = [0x1e61, 0x3062, 0x1e41, 0x1e61, 0x0231,
                          0x0e08, 0x1c0d, 0x3b00, 0x4b00]
        expected = struct.pack('<9H', *expected_words) + b'xY3'
        actual = (run / 'KEYS.BIN').read_bytes()
        if actual != expected:
            raise RuntimeError(f'DOS key mismatch: expected {expected.hex()}, got {actual.hex()}')
        if (run / 'KEYBOARD.OUT').read_text().strip() != 'PI_DOS_KEYBOARD_OK':
            raise RuntimeError('Missing DOS completion marker')
        if 'video kmsdrm' not in (run / 'run.log').read_text().casefold():
            raise RuntimeError('Native KMSDRM driver not confirmed')
        summary = {'virtual_keyboard': name, 'event_node': str(node),
                   'event_owners_at_injection': owners, 'video_driver': 'kmsdrm',
                   'bios_words': [f'{word:04x}' for word in expected_words],
                   'dos_characters': actual[18:].decode('ascii'),
                   'returncode': process.returncode, 'remaining_drm_owners': drm_owners()}
        if summary['remaining_drm_owners']:
            raise RuntimeError('DRM device remained open after DOSBox-X exit')
    finally:
        if process is not None and process.poll() is None:
            os.killpg(process.pid, signal.SIGTERM)
            try:
                process.wait(timeout=3)
            except subprocess.TimeoutExpired:
                os.killpg(process.pid, signal.SIGKILL)
                process.wait(timeout=3)
        try:
            if created:
                fcntl.ioctl(fd, 0x5502)  # UI_DEV_DESTROY: also releases held keys
        finally:
            os.close(fd)
    wait_for(lambda: not node.exists(), seconds=3)
    summary['virtual_device_removed'] = True
    (run / 'result.json').write_text(json.dumps(summary, indent=2) + '\n')
    print('PASS: Linux uinput -> SDL2 evdev -> BIOS scan codes and DOS characters.')
    print('Shift press/release, Enter, Backspace, F1 and Left verified; virtual device removed.')


if __name__ == '__main__':
    try:
        main()
    except (OSError, RuntimeError, subprocess.SubprocessError, ValueError) as error:
        sys.exit(str(error))
