#!/usr/bin/env python3
"""Bounded SSH-compatible KMS rendering diagnostic; never stops display services."""
import json
import os
from pathlib import Path
import shlex
import shutil
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[2]


def drm_owners():
    owners = []
    for process in Path('/proc').iterdir():
        if not process.name.isdigit():
            continue
        try:
            for fd in (process / 'fd').iterdir():
                try:
                    if os.readlink(fd).startswith('/dev/dri/card'):
                        owners.append((process.name, (process / 'comm').read_text().strip()))
                        break
                except OSError:
                    pass
        except OSError:
            pass
    return owners


def validate_pattern(path):
    header, separator, pixels = path.read_bytes().partition(b'\n255\n')
    if not separator or not header.startswith(b'P6\n'):
        raise RuntimeError('invalid GPU readback image')
    width, height = map(int, header.splitlines()[1].split())
    if len(pixels) != width * height * 3:
        raise RuntimeError('GPU readback size mismatch')
    colors = [(255, 0, 0), (0, 255, 0), (0, 0, 255), (255, 255, 0),
              (255, 0, 255), (0, 255, 255), (255, 255, 255), (128, 128, 128)]

    def classify(x, y):
        offset = (y * width + x) * 3
        pixel = pixels[offset:offset+3]
        distances = [max(abs(a-b) for a, b in zip(pixel, color)) for color in colors]
        best = min(range(8), key=distances.__getitem__)
        return best if distances[best] <= 12 else -1

    def bars(y):
        runs = []
        start, previous = 0, classify(0, y)
        for x in range(1, width + 1):
            current = classify(x, y) if x < width else None
            if current != previous:
                if previous >= 0 and x - start >= 3:
                    runs.append((previous, start, x))
                start, previous = x, current
        if [r[0] for r in runs] != list(range(8)):
            raise RuntimeError(f'GPU color-bar mismatch at row {y}: {runs}')
        widths = [r[2] - r[1] for r in runs]
        if min(widths) < 10 or max(widths) - min(widths) > 3:
            raise RuntimeError(f'uneven GPU color bars: {widths}')
        return runs

    middle = bars(height // 2)
    left, right = middle[0][1], middle[-1][2]
    center = (left + right) // 2
    occupied = [y for y in range(height) if classify(center, y) >= 0]
    top, bottom = min(occupied), max(occupied) + 1
    for fraction in (0.25, 0.75):
        bars(top + int((bottom - top) * fraction))
    aspect = (right - left) / (bottom - top)
    return {'width': width, 'height': height, 'pattern_rectangle': [left, top, right, bottom],
            'display_aspect': aspect, 'aspect_4_3': abs(aspect / (4/3) - 1) < 0.02,
            'color_order': ['red', 'green', 'blue', 'yellow', 'magenta', 'cyan', 'white', 'gray']}


def main():
    renderer = os.environ.get('RENDER_DRIVER', 'opengles2')
    if renderer not in ('default', 'opengles2'):
        raise RuntimeError('RENDER_DRIVER must be default or opengles2')
    owners = drm_owners()
    if owners:
        raise RuntimeError(f'DRM devices already in use; leaving services running: {owners}')
    connected = set()
    for status in Path('/sys/class/drm').glob('card*-HDMI-*/status'):
        if status.read_text().strip() == 'connected':
            connected.add(status.parent.name.split('-')[0][4:])
    device = os.environ.get('DRM_DEVICE_INDEX')
    if device is None and len(connected) == 1:
        device = connected.pop()
    if device is None or not device.isdecimal():
        raise RuntimeError('Set DRM_DEVICE_INDEX to the intended connected HDMI DRM card')
    outputs = [p.parent for p in Path('/sys/class/drm').glob(f'card{device}-HDMI-*/status')
               if p.read_text().strip() == 'connected']
    if len(outputs) != 1:
        raise RuntimeError('This diagnostic requires one connected HDMI output on the selected card')
    modes = (outputs[0] / 'modes').read_text().splitlines()
    mode = os.environ.get('HDMI_MODE', modes[0] if modes else '')
    if mode not in modes:
        raise RuntimeError('HDMI_MODE must be a mode advertised by the connected display')
    binary = ROOT / 'output/build/dosbox-x-sdl2/src/dosbox-x'
    if not binary.is_file():
        raise RuntimeError('Build DOSBox-X first with make phase0')
    for tool in ('gcc', 'nasm', 'gdb', 'pkg-config', 'timeout'):
        if not shutil.which(tool):
            raise RuntimeError(f'Missing diagnostic tool: {tool}')
    (ROOT / 'output/smoke').mkdir(parents=True, exist_ok=True)
    run = Path(tempfile.mkdtemp(prefix=f'graphics-{renderer}.', dir=ROOT / 'output/smoke'))
    print('Run directory:', run, flush=True)
    prefix = ROOT / 'output/host-sdl2'
    env = dict(os.environ)
    env['PKG_CONFIG_PATH'] = str(prefix / 'lib/pkgconfig') + ':' + env.get('PKG_CONFIG_PATH', '')
    flags = shlex.split(subprocess.check_output(['pkg-config', '--cflags', '--libs', 'sdl2', 'glesv2'], env=env, text=True))
    probe = run / 'graphics-probe.so'
    subprocess.run(['gcc', '-shared', '-fPIC', '-g', '-Wall', '-Wextra',
                    str(ROOT / 'tools/phase0/graphics-probe.c'), '-o', str(probe),
                    f'-Wl,-rpath,{prefix}/lib', *flags], env=env, check=True)
    subprocess.run(['nasm', '-f', 'bin', str(ROOT / 'tests/video/bars.asm'),
                    '-o', str(run / 'BARS.COM')], check=True)
    for name in ('SDL_RENDER_DRIVER', 'SDL_FRAMEBUFFER_ACCELERATION', 'LD_PRELOAD'):
        env.pop(name, None)
    env.update(SDL_VIDEODRIVER='kmsdrm', SDL_AUDIODRIVER='dummy',
               SDL_KMSDRM_DEVICE_INDEX=device, ARMDOS_CAPTURE_DIR=str(run))
    if renderer != 'default':
        env['SDL_FRAMEBUFFER_ACCELERATION'] = renderer
        env['SDL_RENDER_DRIVER'] = renderer
    command = ['timeout', '--kill-after=5s', '45s', 'gdb', '-q', '-nx', '--batch',
               '--return-child-result', '-ex', f'set environment LD_PRELOAD {probe}',
               '-x', str(ROOT / 'tools/phase0/graphics.gdb'), '--args', str(binary),
               '-defaultconf', '-defaultmapper', '-nopromptfolder', '-defaultdir', str(run),
               '-fastlaunch', '-nomenu', '-fullscreen', '-set', 'sdl output=surface',
               '-set', f'sdl fullresolution={mode}',
               '-set', 'sdl videodriver=kmsdrm', '-set', 'sdl waitonerror=false',
               '-set', 'cpu core=normal', '-set', 'render aspect=true',
               '-c', f'mount c "{run}"', '-c', 'c:', '-c', 'bars.com >GFX.OUT', '-c', 'exit']
    result = subprocess.run(command, cwd=ROOT, env=env, stdout=subprocess.PIPE,
                            stderr=subprocess.STDOUT, text=True, timeout=55)
    (run / 'run.log').write_text(result.stdout)
    if result.returncode:
        print(result.stdout[-6000:])
        raise RuntimeError(f'graphics diagnostic exited {result.returncode}; see {run}/run.log')
    if (run / 'GFX.OUT').read_text().strip() != 'ARMDOS_GRAPHICS_OK':
        raise RuntimeError('DOS graphics program did not complete')
    if 'video kmsdrm' not in result.stdout.casefold():
        raise RuntimeError('native KMSDRM driver not confirmed')
    presentation = json.loads((run / 'presentation.json').read_text())
    metadata = json.loads((run / 'renderer.json').read_text())
    if presentation['capture_result'] or presentation['flips_after_capture'] < 2:
        raise RuntimeError(f'insufficient completed KMS presentation evidence: {presentation}')
    pattern = validate_pattern(run / 'frame.ppm')
    hardware = 'v3d' in metadata['gl_renderer'].casefold() and metadata['renderer_flags'] & 2 != 0
    gles = metadata['gl_version'].startswith('OpenGL ES')
    summary = {'requested_renderer': renderer, 'hdmi_mode': mode, 'renderer': metadata, 'presentation': presentation,
               'pattern': pattern, 'hardware_accelerated': hardware, 'open_gl_es': gles,
               'remaining_drm_owners': drm_owners()}
    (run / 'result.json').write_text(json.dumps(summary, indent=2) + '\n')
    if not hardware or not pattern['aspect_4_3'] or summary['remaining_drm_owners']:
        raise RuntimeError(f'rendering requirement failed; inspect {run}/result.json')
    if renderer == 'opengles2' and not gles:
        raise RuntimeError('OpenGL ES was requested but not active')
    print('PASS: known VGA pattern captured from GPU, 4:3 aspect, completed KMS page flips.', flush=True)
    print(f"SDL renderer: {metadata['sdl_renderer']}; GPU: {metadata['gl_renderer']}; API: {metadata['gl_version']}")
    print(f"Completed flips: {presentation['completed_flips']}; after capture: {presentation['flips_after_capture']}")


if __name__ == '__main__':
    try:
        main()
    except (OSError, RuntimeError, subprocess.SubprocessError, ValueError) as error:
        sys.exit(str(error))
