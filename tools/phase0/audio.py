#!/usr/bin/env python3
"""Bounded DOS audio -> native SDL2 ALSA -> HDMI playback diagnostic."""
from array import array
import json
import math
import os
from pathlib import Path
import re
import signal
import subprocess
import sys
import tempfile
import time
import wave

from graphics import ROOT, drm_owners
from keyboard import event_owners

RATE = 48000


def analyze_pcm(path):
    with wave.open(str(path), 'rb') as capture:
        if (capture.getnchannels(), capture.getsampwidth(), capture.getframerate()) != (2, 2, RATE):
            raise RuntimeError(f'Unexpected ALSA capture format: {capture.getparams()}')
        raw = capture.readframes(capture.getnframes())
    if len(raw) % 4:
        raise RuntimeError('Truncated stereo S16_LE capture')
    samples = array('h')
    samples.frombytes(raw)
    if sys.byteorder != 'little':
        samples.byteswap()
    # Non-overlapping 0.2-second windows. Measure mean-free RMS, crossings,
    # and energy at each expected fundamental; no third-party DSP dependency.
    length = RATE // 5
    windows = []
    tones = {'pc_speaker': 1193182 / 2712, 'adlib': 49716 * 870 / 65536,
             'sound_blaster_dma': 1000.0}
    for start in range(0, len(samples) // 2 - length + 1, length):
        left = samples[start*2:(start+length)*2:2]
        right = samples[start*2+1:(start+length)*2:2]
        mean = sum(left) / length
        data = [value - mean for value in left]
        energy = sum(value*value for value in data)
        rms = math.sqrt(energy / length)
        powers = {}
        for name, frequency in tones.items():
            coefficient = 2 * math.cos(2 * math.pi * frequency / RATE)
            previous = before = 0.0
            for value in data:
                current = value + coefficient * previous - before
                before, previous = previous, current
            power = previous*previous + before*before - coefficient*previous*before
            powers[name] = 2 * max(0, power) / (length * energy) if energy else 0
        crossings = sum(a <= 0 < b for a, b in zip(data, data[1:]))
        winner = max(powers, key=powers.get)
        windows.append({'time_seconds': start / RATE, 'left_rms': rms,
                        'right_rms': math.sqrt(sum(value*value for value in right) / length),
                        'crossing_frequency_hz': crossings * RATE / length,
                        'tone': winner if rms > 20 and powers[winner] > 0.45 else None,
                        'fundamental_energy_fraction': powers[winner]})
    results = {}
    for name, frequency in tones.items():
        matching = [window for window in windows if window['tone'] == name]
        if len(matching) < 4:
            raise RuntimeError(f'{name}: insufficient captured tone evidence ({len(matching)} windows)')
        if any(window['right_rms'] < 20 for window in matching):
            raise RuntimeError(f'{name}: missing right-channel signal')
        representative = max(matching, key=lambda window: window['left_rms'])
        if abs(representative['crossing_frequency_hz'] / frequency - 1) > 0.05:
            raise RuntimeError(f'{name}: representative waveform frequency mismatch')
        results[name] = {'expected_frequency_hz': frequency,
                         'matched_seconds': len(matching) * length / RATE,
                         'representative_crossing_frequency_hz': representative['crossing_frequency_hz'],
                         'representative_fundamental_energy_fraction': representative['fundamental_energy_fraction'],
                         'peak_left_rms': max(w['left_rms'] for w in matching),
                         'first_match_seconds': matching[0]['time_seconds']}
    if not (results['pc_speaker']['first_match_seconds'] < results['adlib']['first_match_seconds']
            < results['sound_blaster_dma']['first_match_seconds']):
        raise RuntimeError('Captured tones are in the wrong order')
    if sum(w['left_rms'] < 5 and w['right_rms'] < 5 for w in windows) < 5:
        raise RuntimeError('Expected silent intervals were not captured')
    return {'rate': RATE, 'channels': 2, 'format': 'S16_LE',
            'duration_seconds': len(raw) / (RATE * 4), 'tones': results, 'windows': windows}


def main():
    if drm_owners():
        raise RuntimeError('DRM device is in use; leaving services running')
    outputs = [p.parent for p in Path('/sys/class/drm').glob('card*-HDMI-*/status')
               if p.read_text().strip() == 'connected']
    if len(outputs) != 1:
        raise RuntimeError('This diagnostic requires one connected HDMI display')
    modes = (outputs[0] / 'modes').read_text().splitlines()
    if not modes:
        raise RuntimeError('Connected display has no advertised modes')
    cards = []
    for card in Path('/proc/asound').glob('card[0-9]*'):
        for eld in card.glob('eld*'):
            if re.search(r'^sad_count\s+[1-9]', eld.read_text(), re.MULTILINE):
                cards.append((card, eld))
    if len(cards) != 1:
        raise RuntimeError('This diagnostic requires one HDMI ALSA card with advertised audio')
    card, eld = cards[0]
    index = card.name[4:]
    card_id = (card / 'id').read_text().strip()
    node = Path(f'/dev/snd/pcmC{index}D0p')
    if event_owners(node):
        raise RuntimeError(f'{node} is already in use; leaving audio services running')
    binary = ROOT / 'output/build/dosbox-x-sdl2/src/dosbox-x'
    if not binary.is_file():
        raise RuntimeError('Build DOSBox-X first with make phase0')
    (ROOT / 'output/smoke').mkdir(parents=True, exist_ok=True)
    run = Path(tempfile.mkdtemp(prefix='audio.', dir=ROOT / 'output/smoke'))
    print('Run directory:', run, flush=True)
    subprocess.run(['nasm', '-f', 'bin', str(ROOT / 'tests/dos/audio.asm'),
                    '-o', str(run / 'AUDIO.COM')], check=True)
    # ALSA file PCM tees the samples to a real slave, not a null device.
    # Only this process uses this config; system and ReplayOS configs stay intact.
    config = run / 'alsa.conf'
    config.write_text(f'''</usr/share/alsa/alsa.conf>
pcm.armdos_capture {{
    type file
    slave.pcm "plughw:CARD={card_id},DEV=0"
    file "{run}/pcm.wav"
    format "wav"
}}
''')
    env = dict(os.environ)
    for name in ('LD_PRELOAD', 'SDL_AUDIO_DEVICE_NAME'):
        env.pop(name, None)
    env.update(SDL_VIDEODRIVER='kmsdrm', SDL_AUDIODRIVER='alsa',
               SDL_KMSDRM_DEVICE_INDEX=outputs[0].name.split('-')[0][4:],
               SDL_RENDER_DRIVER='opengles2', SDL_FRAMEBUFFER_ACCELERATION='opengles2',
               ALSA_CONFIG_PATH=str(config), AUDIODEV='armdos_capture')
    command = [str(binary), '-defaultconf', '-defaultmapper', '-nopromptfolder',
               '-defaultdir', str(run), '-fastlaunch', '-nomenu', '-fullscreen',
               '-set', 'sdl output=surface', '-set', f'sdl fullresolution={modes[0]}',
               '-set', 'sdl videodriver=kmsdrm', '-set', 'sdl waitonerror=false',
               '-set', 'cpu core=normal', '-set', 'cpu cycles=fixed 20000',
               '-set', 'mixer rate=48000', '-set', 'mixer blocksize=1024',
               '-set', 'mixer prebuffer=30', '-set', 'mixer nosound=false',
               '-set', 'speaker pcspeaker=true', '-set', 'sblaster sbtype=sb16',
               '-set', 'sblaster sbbase=220', '-set', 'sblaster dma=1',
               '-set', 'sblaster oplmode=opl2',
               '-c', f'mount c "{run}"', '-c', 'c:', '-c', 'mixer master 25',
               '-c', 'audio.com >AUDIO.OUT', '-c', 'exit']
    observations = []
    process = None
    (run / 'eld.txt').write_text(eld.read_text())
    try:
        with (run / 'run.log').open('w') as log:
            start = time.monotonic()
            process = subprocess.Popen(command, cwd=ROOT, env=env, stdout=log,
                                       stderr=subprocess.STDOUT, start_new_session=True)
            while process.poll() is None:
                if time.monotonic() - start > 30:
                    raise RuntimeError('Audio test exceeded its 30-second deadline')
                entry = {'seconds': time.monotonic() - start,
                         'fixture_active': (run / 'ACTIVE.OK').exists(),
                         'fixture_completed': (run / 'DONE.OK').exists()}
                for name in ('status', 'hw_params'):
                    entry[name] = (card / f'pcm0p/sub0/{name}').read_text()
                observations.append(entry)
                time.sleep(0.05)
    finally:
        if process is not None and process.poll() is None:
            os.killpg(process.pid, signal.SIGTERM)
            try:
                process.wait(timeout=3)
            except subprocess.TimeoutExpired:
                os.killpg(process.pid, signal.SIGKILL)
                process.wait(timeout=3)
        (run / 'hardware-status.json').write_text(json.dumps(observations, indent=2) + '\n')
    if process.returncode:
        raise RuntimeError(f'DOSBox-X exited {process.returncode}; see {run}/run.log')
    if (run / 'AUDIO.OUT').read_text().strip() != 'ARMDOS_AUDIO_OK':
        raise RuntimeError('DOS audio fixture did not finish successfully')
    if not (run / 'DONE.OK').exists() or (run / 'ACTIVE.OK').exists():
        raise RuntimeError('DOS audio fixture lifecycle markers did not complete')
    log = (run / 'run.log').read_text()
    if 'audio alsa' not in log.casefold():
        raise RuntimeError('Native ALSA backend not confirmed')
    running = [entry for entry in observations if 'state: RUNNING' in entry['status']]
    owned = [entry for entry in running if f'owner_pid   : {process.pid}' in entry['status']]
    pointers = [int(match.group(1)) for entry in running
                if (match := re.search(r'hw_ptr\s*:\s*(\d+)', entry['status']))]
    if len(running) < 20 or len(owned) < 20 or len(set(pointers)) < 20:
        raise RuntimeError('Insufficient evidence of hardware playback owned by DOSBox-X')
    xruns = [entry for entry in observations if 'state: XRUN' in entry['status']]
    if any(not entry['fixture_completed'] for entry in xruns):
        raise RuntimeError('An ALSA hardware underrun was observed before the DOS fixture completed')
    signal_result = analyze_pcm(run / 'pcm.wav')
    (run / 'signal.json').write_text(json.dumps(signal_result, indent=2) + '\n')
    summary = {'returncode': process.returncode, 'video_driver': 'kmsdrm',
               'audio_driver': 'alsa', 'hardware_device': str(node), 'alsa_card_id': card_id,
               'alsa_pcm': f'file tee -> plughw:CARD={card_id},DEV=0',
               'hardware_running_observations': len(running),
               'hardware_owned_observations': len(owned),
               'unique_hardware_pointers': len(set(pointers)),
               'hardware_parameters': running[len(running)//2]['hw_params'],
               'observed_playback_xruns': 0, 'observed_teardown_xruns': len(xruns),
               'tones': signal_result['tones'],
               'capture_duration_seconds': signal_result['duration_seconds'],
               'remaining_pcm_owners': event_owners(node),
               'remaining_drm_owners': drm_owners(),
               'audible_speaker_output_verified': False}
    (run / 'result.json').write_text(json.dumps(summary, indent=2) + '\n')
    if summary['remaining_pcm_owners'] or summary['remaining_drm_owners']:
        raise RuntimeError('Audio/display device remained open after exit')
    print('PASS: PC speaker, AdLib and Sound Blaster DMA tones reached native SDL2/ALSA HDMI playback.')
    print(f'Hardware: {card_id}; RUNNING samples: {len(running)}; no sampled playback XRUNs; clean device release.')
    print(f'XRUN observations after the DOS fixture completed: {len(xruns)} (reported separately).')
    print('PCM frequencies validated; audible output has not been checked by a listener.')


if __name__ == '__main__':
    try:
        main()
    except (OSError, RuntimeError, subprocess.SubprocessError, ValueError) as error:
        sys.exit(str(error))
