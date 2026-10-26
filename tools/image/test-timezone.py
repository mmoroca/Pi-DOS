#!/usr/bin/env python3
"""Check user timezone parsing and real winter/summer conversions without root."""
from datetime import datetime, timezone
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]


def main():
    with tempfile.TemporaryDirectory(prefix='pi-dos-timezone.') as directory:
        work = Path(directory)
        config = work / 'PIDOS.CFG'
        worker = work / 'worker.c'
        worker.write_text('''
#include "timezone.h"
#include "config.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
int main(int argc, char **argv) {
    if (argc == 4 && !strcmp(argv[1], "filter"))
        return pi_dos_runtime_config(argv[2], argv[3]) ? 0 : 1;
    if (argc != 3) return 1;
    pi_dos_timezone(argv[1]);
    time_t timestamp = strtoll(argv[2], NULL, 10);
    char output[64];
    if (!strftime(output, sizeof(output), "%H:%M", localtime(&timestamp))) return 1;
    printf("%s|%s\\n", getenv("TZ"), output);
    return 0;
}
''')
        binary = work / 'worker'
        subprocess.run(['gcc', '-O2', '-std=c11', '-Wall', '-Wextra', '-Werror',
                        '-I' + str(ROOT / 'src/init'), str(worker),
                        str(ROOT / 'src/init/timezone.c'), str(ROOT / 'src/init/config.c'),
                        '-o', str(binary)], check=True)

        def check(value, month, expected):
            if value is None:
                config.unlink(missing_ok=True)
            else:
                config.write_bytes(value)
            timestamp = int(datetime(2026, month, 15, 12, tzinfo=timezone.utc).timestamp())
            result = subprocess.run([str(binary), str(config), str(timestamp)],
                                    capture_output=True, text=True, check=True)
            assert result.stdout.strip() == expected, (value, result.stdout, result.stderr)
            if value is not None:
                assert config.read_bytes() == value, 'User configuration was modified'

        check(b'# local clock\r\n[PiDOS]\r\n TIMEZONE = Europe/Madrid # Spain\r\n',
              1, 'Europe/Madrid|13:00')
        check(b'[pidos]\r\ntimezone=Europe/Madrid\r\n', 7, 'Europe/Madrid|14:00')
        check(b'[pidos]\ntimezone=Atlantic/Canary\n', 7, 'Atlantic/Canary|13:00')
        check(b'[pidos]\ntimezone=Asia/Tokyo\n', 1, 'Asia/Tokyo|21:00')
        check(b'[dosbox]\ntimezone=Asia/Tokyo\n', 1, 'Etc/UTC|12:00')
        check(b'[pidos]\ntimezone=Asia/Tokyo\n[pidos]\ntimezone=Europe/Madrid\n',
              1, 'Europe/Madrid|13:00')
        for value in (None, b'', b'# no selection\r\n', b'[pidos]\ntimezone=Unknown/Zone\n',
                      b'[pidos]\ntimezone=../../etc/passwd\n',
                      b'[pidos]\ntimezone=/etc/localtime\n', b'[pidos]\ntimezone=' + b'x' * 1024):
            check(value, 1, 'Etc/UTC|12:00')
        retained = b'[cpu]\r\ncore=normal\r\n[autoexec]\r\necho user commands\r\n'
        config.write_bytes(b'[pidos]\r\ntimezone=Europe/Madrid\r\n' + retained +
                           b'[PIDOS]\r\nfuture-option=value\r\n[dos]\r\nkeyboardlayout=sp\r\n')
        filtered = work / 'runtime.conf'
        subprocess.run([str(binary), 'filter', str(config), str(filtered)], check=True)
        assert filtered.read_bytes() == retained + b'[dos]\r\nkeyboardlayout=sp\r\n'
    print('PASS: custom INI options, winter/summer offsets, UTC fallback, DOSBox filtering and user-file preservation.')


if __name__ == '__main__':
    main()
