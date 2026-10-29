#!/usr/bin/env python3
"""Check the production overlay with the bundled PhysFS implementation and real ZIPs."""
from pathlib import Path
import hashlib
import subprocess
import tempfile
import zipfile

ROOT = Path(__file__).resolve().parents[2]
runtime = next((ROOT / 'output/buildroot-arm64/build').glob('pi-dos-runtime-*'))
physfs = runtime / 'src/libs/physfs'
with tempfile.TemporaryDirectory(prefix='pi-dos-zip.') as temporary:
    work = Path(temporary)
    for filename in ['DOOM.ZIP', 'OTHER.ZIP']:
        with zipfile.ZipFile(work / filename, 'w', zipfile.ZIP_DEFLATED) as archive:
            for name, value in [('DEFAULT.CFG', 'original config'), ('DOOM.EXE', 'executable'),
                                ('DATA/ORIG.DAT', 'original data'), ('EMPTY/', '')]:
                archive.writestr(name, value)
    before = hashlib.sha256((work / 'DOOM.ZIP').read_bytes()).digest()
    objects = []
    for name in ['physfs', 'physfs_archiver_dir', 'physfs_archiver_zip',
                 'physfs_archiver_unpacked', 'physfs_byteorder', 'physfs_platform_posix',
                 'physfs_platform_unix', 'physfs_unicode']:
        obj = work / (name + '.o')
        subprocess.run(['cc', '-O1', '-DPHYSFS_SUPPORTS_DEFAULT=0', '-DPHYSFS_SUPPORTS_ZIP=1',
                        '-c', str(physfs / (name + '.c')), '-o', str(obj)], check=True)
        objects.append(str(obj))
    executable = work / 'check'
    # Exercise the actual DOS adapter's path conversion too: directory-cache
    # callbacks pass full virtual paths, while file calls pass DOS relative paths.
    drive = (ROOT / 'src/runtime/zip_drive.h').read_text()
    start = drive.index('    bool resolve(')
    method = drive[start:drive.index('    void changed()', start)]
    upstream = (runtime / 'src/dos/drive_physfs.cpp').read_text()
    start = upstream.index('static char *normalize(')
    normalize = upstream[start:upstream.index('\n}', start) + 2]
    (work / 'zip-path-method.h').write_text(normalize + '\nclass PathProbe {\n'
        '    std::string mountpoint = "E_DRIVE";\n'
        '    char basedir[16] = "\\\\E_DRIVE\\\\";\n'
        '    struct { void ExpandName(char *) {} } dirCache;\n'
        'public:\n' + method + '};\n')
    subprocess.run(['g++', '-std=c++14', '-Wall', '-Wextra', '-Werror',
                    '-I', str(work), '-I', str(physfs), '-I', str(ROOT / 'src/runtime'),
                    str(ROOT / 'tools/image/zip-overlay-test.cpp'), *objects,
                    '-lpthread', '-o', str(executable)], check=True)
    subprocess.run([str(executable), str(work)], check=True)
    assert hashlib.sha256((work / 'DOOM.ZIP').read_bytes()).digest() == before
