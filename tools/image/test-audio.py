#!/usr/bin/env python3
"""Check physical audio discovery with sysfs/procfs fixtures; no hardware needed."""
import ctypes
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]

with tempfile.TemporaryDirectory(prefix='pi-dos-audio.') as directory:
    work = Path(directory)
    library = work / 'audio.so'
    subprocess.run(['cc', '-std=c11', '-Wall', '-Wextra', '-Werror', '-shared', '-fPIC',
                    str(ROOT/'src/init/audio.c'), str(ROOT/'src/init/config.c'),
                    '-I'+str(ROOT/'src/common'), '-o', str(library)], check=True)
    audio = ctypes.CDLL(str(library))
    audio.pi_dos_audio_find.argtypes = [ctypes.c_char_p] * 4 + [ctypes.c_size_t]
    audio.pi_dos_audio_find.restype = ctypes.c_bool
    sound, asound = work/'sound', work/'asound'
    sound.mkdir()
    asound.mkdir()

    def card(index, identity, playback=(0,), usb=False, driver=None):
        target = sound/f'card{index}'
        target.mkdir()
        (target/'id').write_text(identity+'\n')
        for pcm in playback:
            (sound/f'pcmC{index}D{pcm}p').touch()
        if usb:
            target = asound/f'card{index}'
            target.mkdir()
            (target/'usbbus').write_text('001/002\n')
        if driver:
            target = sound/f'card{index}'/'device'
            target.mkdir()
            (target/'driver').symlink_to('/sys/bus/vchiq/drivers/'+driver)

    def find(output, expected):
        device = ctypes.create_string_buffer(128)
        found = audio.pi_dos_audio_find(output.encode(), str(sound).encode(),
                                       str(asound).encode(), device, len(device))
        assert found == (expected is not None), (output, found, device.value)
        if found:
            assert device.value == expected.encode(), (output, device.value)

    find('hdmi', None)
    card(0, 'vc4hdmi1')
    card(1, 'Headphones', usb=True)  # USB headset must not impersonate the Pi jack.
    card(2, 'Capture', playback=(), usb=True)
    (sound/'pcmC2D0c').touch()
    card(3, 'OnboardDAC')  # Non-USB sound cards are not USB candidates.
    find('hdmi', None)  # No fallback to HDMI1, USB or another onboard card.
    find('jack', None)
    find('usb', 'plughw:CARD=Headphones,DEV=0')
    card(9, 'vc4hdmi')
    find('hdmi', 'plughw:CARD=vc4hdmi,DEV=0')
    card(4, 'vc4hdmi0')
    find('HDMI', 'plughw:CARD=vc4hdmi0,DEV=0')
    card(5, 'Headphones_1', driver='bcm2835-audio')
    find('jack', 'plughw:CARD=Headphones_1,DEV=0')
    card(8, 'DAC', playback=(10, 2), usb=True)
    find('USB', 'plughw:CARD=DAC,DEV=2')  # Playback need not be device zero.
    # Numeric card enumeration changes must preserve the same ALSA destination.
    (sound/'card8').rename(sound/'card12')
    (asound/'card8').rename(asound/'card12')
    for pcm in (10, 2):
        (sound/f'pcmC8D{pcm}p').rename(sound/f'pcmC12D{pcm}p')
    find('usb', 'plughw:CARD=DAC,DEV=2')
    find('invalid', None)
    device = ctypes.create_string_buffer(8)
    assert not audio.pi_dos_audio_find(b'usb', str(sound).encode(), str(asound).encode(),
                                       device, len(device))

print('PASS: HDMI, Pi jack and USB playback discovery, isolation and stable ALSA IDs')
