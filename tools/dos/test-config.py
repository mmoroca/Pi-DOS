#!/usr/bin/env python3
"""Exercise the production config editor's preservation and recovery behavior."""
import ctypes
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

print('PASS: custom/sparse configs, duplicate keys, byte preservation, backup, recovery and size limits.')
