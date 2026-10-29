#!/usr/bin/env python3
"""Exercise the production DOS file API and sharing table with simulated PSPs/media."""
from pathlib import Path
import resource
import subprocess
import tempfile

resource.setrlimit(resource.RLIMIT_CORE, (0, 0))
root = Path(__file__).resolve().parents[2]
runtime = next((root / 'output/buildroot-arm64/build').glob('pi-dos-runtime-*'))
source = (runtime / 'src/dos/dos_files.cpp').read_text()
parts = []
for signature in ['bool DOS_UsesInternalSharing(', 'void DOS_ReleaseSharingLocks(']:
    start = source.index(signature)
    parts.append(source[start:source.index('\n}', start) + 2])
for signature in ['static bool PiDosShareError(', 'static bool PiDosShareKey(', 'static bool PiDosShareCurrent(',
                  'static bool PiDosShareAccess(', 'bool DOS_ReadFile(', 'bool DOS_WriteFile(',
                  'bool DOS_SeekFile(', 'bool DOS_LockFile(', 'bool DOS_CloseFile(',
                  'bool DOS_CreateFile(', 'bool DOS_OpenFile(', 'bool DOS_OpenFileExtended(',
                  'bool DOS_UnlinkFile(', 'bool DOS_Rename(', 'bool DOS_DuplicateEntry(',
                  'bool DOS_ForceDuplicateEntry(']:
    start = source.index(signature)
    parts.append(source[start:source.index('\n}', start) + 2])
classes = (runtime / 'src/dos/dos_classes.cpp').read_text()
start = classes.index('bool DOS_SharingSFNPath(')
parts.insert(0, classes[start:classes.index('\n}', start) + 2])
assert 'DOS_ReleaseSharingLocks(pspseg)' in (runtime / 'src/dos/dos_execute.cpp').read_text()
assert 'if (!DOS_UsesInternalSharing() && Files[i]' in source
assert 'dos_sharing.clear();' in source
with tempfile.TemporaryDirectory(prefix='pi-dos-sharing.') as temporary:
    work = Path(temporary)
    (work / 'methods.h').write_text('\n\n'.join(parts))
    subprocess.run(['g++', '-std=c++14', '-DLINUX', '-O1', '-Wall', '-Wextra', '-Werror',
                    '-I', str(work), '-I', str(root / 'src/runtime'),
                    str(root / 'tools/image/dos-sharing-test.cpp'), '-o', str(work / 'check')], check=True)
    subprocess.run([str(work / 'check'), str(work / 'metadata')], check=True)
