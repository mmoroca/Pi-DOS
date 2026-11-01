#!/usr/bin/env python3
"""Run the real local-drive and ATTRIB methods with a simulated FAT ioctl interface.

Uses temporary ordinary files, never mounts or accesses a host block device.
"""
from pathlib import Path
import subprocess
import tempfile
import resource

resource.setrlimit(resource.RLIMIT_CORE, (0, 0))

root = Path(__file__).resolve().parents[2]
runtime = next((root / 'output/buildroot-arm64/build').glob('pi-dos-runtime-*'))
local = (runtime / 'src/dos/drive_local.cpp').read_text()
shell = (runtime / 'src/shell/shell_cmds.cpp').read_text()
parts = []
for signature in ['static bool PiDosAttributeError(', 'static bool PiDosSyncParent(', 'bool localDrive::FileCreate(',
                  'bool localDrive::FileOpen(', 'bool localDrive::FileUnlink(',
                  'bool localDrive::SetFileAttr(', 'bool localDrive::GetFileAttr(',
                  'bool localDrive::FindFirst(', 'bool localDrive::FindNext(',
                  'bool localDrive::MakeDir(', 'bool localDrive::RemoveDir(', 'bool localDrive::Rename(',
                  'bool LocalFile::Read(', 'bool LocalFile::Write(', 'bool LocalFile::SyncNative(',
                  'bool LocalFile::Close(', 'void LocalFile::Flush(']:
    start = local.index(signature)
    parts.append(local[start:local.index('\n}', start) + 2])
for signature in ['static bool doAttrib(', 'void DOS_Shell::CMD_ATTRIB(', 'void DOS_Shell::CMD_DELETE(']:
    start = shell.index(signature)
    parts.append(shell[start:shell.index('\n}', start) + 2])
with tempfile.TemporaryDirectory(prefix='pi-dos-attributes.') as temporary:
    work = Path(temporary)
    (work / 'methods.h').write_text('\n'.join(parts))
    subprocess.run(['g++', '-std=c++17', '-DLINUX', '-Wall', '-Wextra', '-Werror', '-Wno-missing-field-initializers',
                    '-I', str(work), '-I', str(root / 'src/runtime'),
                    str(root / 'tools/image/dos-attributes-test.cpp'), '-o', str(work / 'check')], check=True)
    subprocess.run([str(work / 'check'), str(work)], check=True)
