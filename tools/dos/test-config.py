#!/usr/bin/env python3
"""Exercise the production config editor's preservation and recovery behavior."""
import ctypes
import re
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]
with tempfile.TemporaryDirectory(prefix='pidoscfg-config.') as temporary:
    temporary = Path(temporary)
    library = temporary / 'config.so'
    subprocess.run(['cc', '-std=c89', '-pedantic', '-Wall', '-Wextra', '-Werror',
                    '-shared', '-fPIC', '-I' + str(ROOT / 'src/common'),
                    str(ROOT / 'src/doscfg/config.c'), str(ROOT / 'src/common/cpu_profiles.c'),
                    str(ROOT / 'src/common/video_profiles.c'),
                    str(ROOT / 'src/common/midi_profiles.c'),
                    '-o', str(library)], check=True)
    editor = ctypes.CDLL(str(library))
    editor.cfg_load.argtypes = [ctypes.c_char_p]
    editor.cfg_save.argtypes = [ctypes.c_char_p, ctypes.c_int]
    assert editor.cpu_profile_default() == 7
    assert editor.cfg_load(str(ROOT / 'configs/pi-dos.conf').encode()) == 1
    assert editor.cfg_profile() == editor.cpu_profile_default()
    path = temporary / 'PIDOS.CFG'
    encoded = str(path).encode()

    # Duplicate sections/keys are updated consistently; unrelated bytes survive.
    before = (b'# Keep this comment\r\n[pidos]\r\ntimezone=Europe/Madrid\r\n'
              b'[CPU]\r\n  CPUTYPE =386\r\ncycles=6075\r\n'
              b'# CPU comment\r\n[dosbox]\r\nmemsize=32\r\n'
              b'[cpu]\r\ncycles=auto\r\n#DOSBOX-X:core=normal\r\n'
              b'[autoexec]\r\necho core=unchanged\r\n')
    path.write_bytes(before)
    assert editor.cfg_load(encoded) == 1
    assert editor.cfg_profile() == -1
    assert editor.cfg_save(encoded, 4) == 1  # 486DX2 66 MHz
    expected = before.replace(b'CPUTYPE =386', b'CPUTYPE =486').replace(
        b'cycles=6075', b'cycles=fixed 23880').replace(
        b'cycles=auto', b'cycles=fixed 23880').replace(
        b'#DOSBOX-X:core=normal', b'#DOSBOX-X:core=dynamic_rec')
    assert path.read_bytes() == expected
    assert path.with_suffix('.BAK').read_bytes() == before
    assert editor.cfg_profile() == 4
    assert editor.cfg_save(encoded, 4) == 1
    assert path.read_bytes() == expected  # no growing duplicate sections

    # Sparse/empty configs inherit the shipped defaults; missing CPU keys append.
    sparse = b'[dosbox]\nmemsize=64\n[autoexec]\necho hello'
    path.write_bytes(sparse)
    assert editor.cfg_load(encoded) == 1 and editor.cfg_profile() == 7
    assert editor.cfg_save(encoded, 6) == 1  # Pentium 100 MHz
    assert path.read_bytes().startswith(sparse + b'\r\n')
    assert editor.cfg_profile() == 6
    assert b'cycles=fixed 53430\r\n' in path.read_bytes()

    # Saving reloads the file, retaining changes made after the UI was opened.
    assert editor.cfg_load(encoded) == 1
    updated = path.read_bytes() + b'[dos]\r\nxms=false\r\n'
    path.write_bytes(updated)
    assert editor.cfg_save(encoded, 0) == 1
    assert b'[dos]\r\nxms=false\r\n' in path.read_bytes()
    assert path.with_suffix('.BAK').read_bytes() == updated

    # A recovery file or an unwritable backup must never damage the original.
    stable = path.read_bytes()
    path.with_suffix('.TMP').write_bytes(b'recovery')
    assert editor.cfg_save(encoded, 1) == 0
    assert path.read_bytes() == stable
    assert path.with_suffix('.TMP').read_bytes() == b'recovery'
    path.with_suffix('.TMP').unlink()
    path.with_suffix('.BAK').unlink()
    path.with_suffix('.BAK').mkdir()
    (path.with_suffix('.BAK') / 'keep').write_bytes(b'not a backup file')
    assert editor.cfg_save(encoded, 1) == 0
    assert path.read_bytes() == stable
    assert not path.with_suffix('.TMP').exists()
    (path.with_suffix('.BAK') / 'keep').unlink()
    path.with_suffix('.BAK').rmdir()

    # Reject unsafe inputs, including an output that would exceed the size cap.
    for bad in (b'[cpu]\ncycles=auto\x00hidden', b'#' * 24577):
        path.write_bytes(bad)
        assert editor.cfg_load(encoded) == 0
        assert editor.cfg_save(encoded, 1) == 0
        assert path.read_bytes() == bad
    full = b'#' * 24576
    path.write_bytes(full)
    assert editor.cfg_load(encoded) == 1
    assert editor.cfg_save(encoded, 1) == 0
    assert path.read_bytes() == full and not path.with_suffix('.TMP').exists()
    path.unlink()
    assert editor.cfg_load(encoded) == 1 and editor.cfg_profile() == 7
    assert editor.cfg_save(encoded, 7) == 1
    assert editor.cfg_profile() == 7
    assert b'cputype=pentium\r\n' in path.read_bytes()
    assert b'cycles=fixed 71240\r\n' in path.read_bytes()
    assert editor.cfg_save(encoded, -1) == 0
    assert editor.cfg_save(encoded, 9) == 0
    assert editor.cfg_save(str(path.with_suffix('.BAK')).encode(), 0) == 0

    # Exercise production settings APIs; only explicitly edited fields persist.
    header = (ROOT / 'src/doscfg/config.h').read_text()
    names = re.search(r'enum \{\s*(K_CPU_TYPE.*?)\};', header, re.S)[1]
    keys = {name.strip(): index for index, name in enumerate(names.split(','))}
    editor.cfg_get.restype = ctypes.c_char_p
    editor.cfg_set.argtypes = [ctypes.c_int, ctypes.c_char_p]
    editor.cfg_save_changes.argtypes = [ctypes.c_char_p]
    def get(name): return editor.cfg_get(keys[name])
    def set_value(name, value): return editor.cfg_set(keys[name], value)
    before = (ROOT / 'configs/pi-dos.conf').read_bytes() + b'\n[pidos]\ntimezone=Europe/Madrid\n# keep\n'
    for card, sb, opl, gus, tandy in (
        (0,b'none',b'none',b'false',b'off'), (1,b'none',b'none',b'false',b'on'),
        (2,b'none',b'opl2',b'false',b'off'), (3,b'gb',b'none',b'false',b'off'),
        (4,b'sb2.0',b'opl2',b'false',b'off'), (5,b'sbpro2',b'opl3',b'false',b'off'),
        (6,b'sb16',b'auto',b'false',b'off'), (7,b'none',b'none',b'true',b'off')):
        path.write_bytes(before)
        assert editor.cfg_load(encoded)
        editor.cfg_select_card(card)
        assert editor.cfg_card() == card
        assert (get('K_SB_TYPE'),get('K_OPL'),get('K_GUS'),get('K_TANDY')) == (sb,opl,gus,tandy)
        assert get('K_SPEAKER') == b'true'
    assert set_value('K_RAM', b'128')
    assert set_value('K_SPEAKER', b'false')
    assert set_value('K_VOL_MASTER', b'65:75')
    assert set_value('K_KEYBOARD', b'us')
    assert set_value('K_COUNTRY', b'1,437')
    assert set_value('K_TIMEZONE', b'Asia/Tokyo')
    assert editor.cfg_save_changes(encoded)
    data = path.read_bytes()
    for assignment in (b'memsize=128',b'pcspeaker=false',b'mixer_master=65:75',
                       b'keyboardlayout=us',b'country=1,437',b'timezone=Asia/Tokyo'):
        assert assignment + b'\r\n' in data
    assert b'# keep\n' in data and b'lfn=true\n' in data and b'cycles=fixed 71240\n' in data
    for key, value in (('K_RAM',b'3'),('K_GUS_PORT',b'270'),('K_VOL_MASTER',b'201'),
                       ('K_VOL_MASTER',b'50:20 & echo BAD'),('K_TIMEZONE',b'../etc/passwd')):
        assert not set_value(key,value)
    assert editor.cfg_select_cpu(1)  # 286 cannot address 128 MiB
    assert not editor.cfg_save_changes(encoded)
    assert path.read_bytes() == data
    assert editor.cfg_load(encoded)
    editor.cfg_seed(keys['K_VOL_MASTER'], b'60:70')
    assert get('K_VOL_MASTER') == b'60:70' and not editor.cfg_changed()
    assert editor.cfg_save_changes(encoded) and path.read_bytes() == data

    # Video selections and the complete Voodoo preset share PIDOS.CFG.
    assert get('K_VOODOO') == b'software' and get('K_DIAGNOSTICS') == b'false'
    for video in (b'cga',b'hercules',b'tandy',b'ega',b'vgaonly',b'svga_et4000',
                  b'svga_s3',b'svga_dosbox',b'svga_dosbox_vbe2'):
        assert set_value('K_MACHINE', video)
    assert not set_value('K_MACHINE', b'invalid-card')
    editor.cfg_toggle_voodoo()
    assert get('K_VOODOO') == b'false'
    assert get('K_VOODOO_MEM') == b'true' and get('K_GLIDE') == b'false'
    assert set_value('K_DIAGNOSTICS', b'true')
    assert editor.cfg_save_changes(encoded)
    for setting in (b'machine=svga_dosbox_vbe2',b'voodoo_card=false',
                    b'voodoo_maxmem=true',b'glide=false',b'diagnostics=true'):
        assert setting+b'\r\n' in path.read_bytes()
    editor.cfg_toggle_voodoo()
    assert get('K_VOODOO') == b'software'
    editor.cfg_reset_page(2)  # Video
    assert get('K_MACHINE') == b'svga_s3'
    editor.cfg_reset_page(5)  # System
    assert get('K_DIAGNOSTICS') == b'false'

    # Every MIDI preset persists its complete backend alongside independent SB settings.
    for index, profile, device in ((0,b'none',b'none'),(1,b'sc55',b'synth'),
                                   (2,b'roland-gm',b'synth'),(3,b'mt32',b'mt32'),
                                   (4,b'cm32l',b'mt32'),(5,b'external',b'alsa')):
        assert editor.cfg_load(encoded)
        sb = get('K_SB_TYPE')
        editor.cfg_select_midi(index)
        assert get('K_MIDI_PROFILE') == profile and get('K_MIDI_DEVICE') == device
        assert get('K_SB_TYPE') == sb
        if index:
            assert get('K_MPU') == b'intelligent'
        if index == 5:
            assert not editor.cfg_save_changes(encoded)
            assert set_value('K_MIDI_PORT',b'pidos:0123456789abcdef')
        assert editor.cfg_save_changes(encoded)
        assert b'midi_profile='+profile+b'\r\n' in path.read_bytes()
    for value in (b'128:0',b'pidos:xyz',b'pidos:0123456789abcdeg'):
        assert not set_value('K_MIDI_PORT',value)

print('PASS: all cards, RAM/region/mixer settings, validation, preservation, backups and recovery.')
