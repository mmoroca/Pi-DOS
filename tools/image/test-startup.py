#!/usr/bin/env python3
"""Check CPU summary overrides and the 80-column boot table layout."""
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]

with tempfile.TemporaryDirectory(prefix='pi-dos-startup.') as directory:
    work = Path(directory)
    wrapper = work / 'startup.c'
    wrapper.write_text('''
#include "startup.h"
int main(int argc, char **argv) {
    if (argc != 3) return 1;
    pi_dos_startup(stdout, argv[1], argv[2], "/dev/test", false, false, false);
    return 0;
}
''')
    binary = work / 'startup'
    subprocess.run(['cc', '-std=c11', '-Wall', '-Wextra', '-Werror',
                    '-I' + str(ROOT / 'src/init'), '-I' + str(ROOT / 'src/common'),
                    str(wrapper), str(ROOT / 'src/init/startup.c'),
                    str(ROOT / 'src/common/cpu_profiles.c'), '-o', str(binary)], check=True)
    defaults = ROOT / 'configs/pi-dos.conf'
    user = work / 'PIDOS.CFG'

    def check(config, cpu, cycles):
        user.write_bytes(config)
        output = subprocess.check_output([str(binary), str(defaults), str(user)])
        lines = output.splitlines()
        date_row = next(line for line in lines if b' Date ' in line)
        time_row = next(line for line in lines if b' Time ' in line)
        assert lines.index(time_row) == lines.index(date_row) + 1
        cpu_row = next(line for line in lines if b' DOS CPU ' in line)
        video_row = next(line for line in lines if b' DOS video ' in line)
        assert lines.index(cpu_row) + 1 == lines.index(video_row)
        assert b' DOS CPU    : ' + cpu in cpu_row
        assert time_row[5 + 38:-1].strip() == b''
        assert b'DOS cycles' not in output
        assert b'~' not in cpu_row
        for line in lines:
            if line.startswith(b'ECHO \xba'):
                assert len(line[5:]) == 75, line
                assert b'|' not in line and b'>' not in line and b'<' not in line
        assert user.read_bytes() == config
        return output

    check(b'', b'Pentium 133 MHz', b'fixed 71240')
    check(b'[cpu]\ncycles=fixed 6075\n', b'Pentium ', b'fixed 6075')
    check(b'[cpu]\ncputype=auto\ncycles=auto\n', b'Auto (486/Pentium)', b'auto')
    check(b'[cpu]\ncputype=pentium\ncycles=fixed 71240\n', b'Pentium 133 MHz', b'fixed 71240')
    check(b'[cpu]\ncputype=486\ncycles=fixed 23880\n', b'486DX2 66 MHz', b'fixed 23880')
    check(b'[cpu]\ncputype=486\ncycles=33445\n', b'486DX4 100 MHz', b'33445')
    check(b'[cpu]\ncputype=386\ncycles=fixed 9000\n', b'386 ', b'fixed 9000')
    result = check(b'[cpu]\ncputype=pentium_mmx\ncycles=fixed 114657\n',
                   b'Pentium MMX 200 MHz', b'fixed 114657')
    output = ROOT / 'output/doscfg'
    output.mkdir(parents=True, exist_ok=True)
    (output / 'boot-table-example.bat').write_bytes(result.replace(b'\n', b'\r\n'))

print('PASS: Date/Time in left column, DOS CPU immediately above DOS video, no cycles; defaults, profiles, custom settings and 75-column width.')
