#!/usr/bin/env python3
"""Exercise PID 1 logging against disposable FIFO/proc fixtures, without root."""
import os
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]


def main():
    with tempfile.TemporaryDirectory(prefix='pi-dos-diagnostics.') as temporary:
        work = Path(temporary)
        config, devices = work / 'PIDOS.CFG', work / 'devices'
        boot, runtime, kernel = work / 'BOOT.LOG', work / 'RUNTIME.LOG', work / 'kmsg'
        directory = work / 'fds/123'
        directory.mkdir(parents=True)
        (directory / '5').symlink_to('/dev/input/event7')
        devices.write_text('N: Name="Test keyboard"\nH: Handlers=kbd event7\n')
        os.mkfifo(kernel)
        stream = os.open(kernel, os.O_RDWR | os.O_NONBLOCK)
        source = (ROOT / 'src/init/diagnostics.c').read_text()
        replacements = {
            '/proc/bus/input/devices': str(devices),
            '/proc/%ld/fd/*': str(work / 'fds/%ld/*'),
            '/dosdata/SYSTEM/BOOT.LOG': str(boot),
            '/dosdata/SYSTEM/RUNTIME.LOG': str(runtime),
            '/dev/kmsg': str(kernel),
        }
        for original, replacement in replacements.items():
            assert original in source
            source = source.replace(original, replacement)
        (work / 'diagnostics.c').write_text(source)
        (work / 'worker.c').write_text(r'''
#include "diagnostics.c"
#include <assert.h>
#include <sys/wait.h>
int main(int argc, char **argv)
{
    assert(argc >= 3);
    struct pi_dos_diagnostics log;
    bool enabled = pi_dos_diagnostics_enabled(argv[2]);
    pi_dos_diagnostics_start(&log, enabled, atoi(argv[1]) != 0);
    if (argc > 3) {
        pid_t child = fork();
        assert(child >= 0);
        if (!child) {
            pi_dos_diagnostics_redirect(&log);
            dprintf(1, "runtime stdout captured\n");
            dprintf(2, "runtime stderr captured\n");
            _exit(0);
        }
        int status;
        assert(waitpid(child, &status, 0) == child && status == 0);
    }
    puts("READY"); fflush(stdout);
    int command;
    while ((command = getchar()) != EOF && command != 'q') {
        if (command == '\n') continue;
        if (command == 'c') {
            char *data = malloc(BOOT_LOG_LIMIT + 1);
            assert(data);
            memset(data, 'X', BOOT_LOG_LIMIT + 1);
            append(&log, data, BOOT_LOG_LIMIT + 1);
            free(data);
        }
        log.next_snapshot = log.next_sync = 0;
        pi_dos_diagnostics_poll(&log, 123);
        puts("READY"); fflush(stdout);
    }
    int descriptors[] = {log.kernel_fd, log.boot_fd, log.runtime_fd};
    pi_dos_diagnostics_stop(&log);
    for (size_t i = 0; i < sizeof(descriptors) / sizeof(descriptors[0]); ++i)
        if (descriptors[i] >= 0) assert(fcntl(descriptors[i], F_GETFD) < 0 && errno == EBADF);
    puts("CLOSED");
    return 0;
}
''')
        subprocess.run(['gcc', '-std=c11', '-Wall', '-Wextra', '-Werror',
                        '-I' + str(ROOT / 'src/init'), str(work / 'worker.c'),
                        str(ROOT / 'src/init/config.c'),
                        '-o', str(work / 'worker')], check=True)

        def start(writable=True, redirect=False):
            args = [str(work / 'worker'), str(int(writable)), str(config)]
            if redirect:
                args.append('redirect')
            child = subprocess.Popen(args, stdin=subprocess.PIPE, stdout=subprocess.PIPE,
                                     stderr=subprocess.PIPE, text=True)
            assert child.stdout.readline().strip() == 'READY'
            return child

        def command(child, value):
            child.stdin.write(value + '\n')
            child.stdin.flush()
            assert child.stdout.readline().strip() == 'READY'

        def stop(child):
            output, error = child.communicate('q\n', timeout=5)
            assert child.returncode == 0, error
            assert output.strip() == 'CLOSED', output

        try:
            for setting in (None, '', '[pidos]\ndiagnostics=false\n',
                            '[sdl]\ndiagnostics=true\n', '[pidos]\ndiagnostics=invalid\n',
                            '[pidos]\ndiagnostics=' + 'x' * 100 + '\n',
                            '[pidos]\ndiagnostics=true\ndiagnostics=false\n'):
                if setting is None:
                    config.unlink(missing_ok=True)
                else:
                    config.write_text(setting)
                boot.write_text('keep boot')
                runtime.write_text('keep runtime')
                stop(start())
                assert boot.read_text() == 'keep boot' and runtime.read_text() == 'keep runtime'
            config.write_bytes(b'[PiDOS]\r\n diagnostics = TRUE # capture\r\n')
            stop(start(writable=False))
            assert boot.read_text() == 'keep boot' and runtime.read_text() == 'keep runtime'
            boot.unlink()
            runtime.unlink()
            os.write(stream, b'6,1,100,-;early USB controller initialization\n')
            child = start(redirect=True)
            assert b'early USB controller' in boot.read_bytes()
            assert 'runtime stdout captured' in runtime.read_text()
            assert 'runtime stderr captured' in runtime.read_text()
            command(child, 'p')
            initial = boot.read_bytes()
            assert b'Test keyboard' in initial and b'/dev/input/event7' in initial
            command(child, 'p')
            assert boot.read_bytes() == initial, 'Repeated identical snapshot was logged'
            os.write(stream, b'6,2,200,-;USB keyboard reconnected\n')
            devices.write_text('N: Name="Reconnected keyboard"\nH: Handlers=kbd event8\n')
            command(child, 'p')
            assert b'USB keyboard reconnected' in boot.read_bytes()
            assert b'Reconnected keyboard' in boot.read_bytes()
            command(child, 'c')
            assert 4 * 1024**2 <= boot.stat().st_size < 4 * 1024**2 + 100
            assert boot.read_bytes().endswith(b'capture stopped]\n')
            stop(child)
        finally:
            os.close(stream)
    print('PASS: PIDOS.CFG opt-in/read-only behavior, backlog/live capture, input snapshots, runtime output, size limit and descriptor cleanup.')


if __name__ == '__main__':
    main()
