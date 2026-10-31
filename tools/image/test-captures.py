#!/usr/bin/env python3
"""Check the runtime's screenshot completion path using temporary files only."""
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]
runtime = next((ROOT / 'output/buildroot-arm64/build').glob('pi-dos-runtime-*'))
hardware = (runtime / 'src/hardware/hardware.cpp').read_text()
start = hardware.index('static bool CloseScreenshotFile(')
method = hardware[start:hardware.index('\n}', start) + 2]

with tempfile.TemporaryDirectory(prefix='pidos-captures.') as temporary:
    work = Path(temporary)
    (work / 'close.h').write_text(method)
    (work / 'check.cpp').write_text(r'''
#include <cassert>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <fcntl.h>
#include <string>
#include <sys/stat.h>
#include <unistd.h>
#include "drive_policy.h"
struct Drive {
    int refreshes = 0;
    void EmptyCache() { ++refreshes; }
};
static Drive drive, *Drives[26] = {};
static std::string capturedir, pathscr, calls;
static char failure = 0;
static int errors = 0;
#define LOG_MSG(...) (++errors)
extern "C" int __real_fsync(int);
extern "C" int __real_fflush(FILE *);
extern "C" int __real_fclose(FILE *);
extern "C" int __wrap_fsync(int fd) {
    struct stat status = {};
    assert(!fstat(fd, &status));
    char operation = S_ISDIR(status.st_mode) ? 'D' : 'F';
    calls += operation;
    if (failure == operation) { errno = EIO; return -1; }
    return __real_fsync(fd);
}
extern "C" int __wrap_fflush(FILE *file) {
    calls += 'W';
    int result = __real_fflush(file);
    if (failure == 'W') { errno = ENOSPC; return EOF; }
    return result;
}
extern "C" int __wrap_fclose(FILE *file) {
    calls += 'C';
    int result = __real_fclose(file);
    if (failure == 'C') { errno = EIO; return EOF; }
    return result;
}
#include "close.h"
static void capture(bool expected) {
    calls.clear();
    FILE *file = fopen(pathscr.c_str(), "wb");
    assert(file);
    assert(fputs("capture contents", file) >= 0);
    int fd = fileno(file);
    const int previous_errors = errors;
    assert(CloseScreenshotFile(file) == expected);
    assert(errors == previous_errors + !expected);
    errno = 0;
    assert(fcntl(fd, F_GETFD) == -1 && errno == EBADF);
}
int main(int argc, char **argv) {
    assert(argc == 2);
    capturedir = argv[1];
    pathscr = capturedir + "/shot_000.png";
    Drives[2] = &drive;
    setenv("PI_DOS_SUPERVISED", "1", 1);
    capture(true);
    // Buffered contents reach the file before its sync, then the directory sync.
    assert(calls == "WFCD" && drive.refreshes == 1);
    FILE *readback = fopen(pathscr.c_str(), "rb");
    assert(readback);
    char buffer[32] = {};
    assert(fread(buffer, 1, sizeof(buffer), readback) == 16);
    assert(std::string(buffer) == "capture contents");
    assert(!fclose(readback));
    for (char operation : std::string("WFCD")) {
        failure = operation;
        capture(false);
        assert(calls == "WFCD");
    }
    assert(drive.refreshes == 5);
    failure = 0;
    // A failed directory open must also prevent a successful save result.
    const std::string directory = capturedir;
    capturedir += "/missing";
    capture(false);
    assert(calls == "WFC" && drive.refreshes == 6);
    capturedir = directory;
    // A prior stdio error must survive a subsequent successful fflush.
    FILE *full = fopen("/dev/full", "wb");
    assert(full && !setvbuf(full, nullptr, _IONBF, 0));
    assert(fputs("capture", full) == EOF && ferror(full));
    unsetenv("PI_DOS_SUPERVISED");
    assert(!CloseScreenshotFile(full));
    // Standalone runs do not sync appliance storage or invalidate C:.
    capture(true);
    assert(calls == "WC" && drive.refreshes == 6);
    setenv("PI_DOS_SUPERVISED", "1", 1);
    Drives[2] = nullptr;
    capture(true);
    assert(calls == "WFCD");
}
''')
    binary = work / 'check'
    subprocess.run(['c++', '-std=c++14', '-DLINUX', '-Wall', '-Wextra', '-Werror',
                    '-I' + str(ROOT / 'src/runtime'), '-I' + str(work),
                    str(work / 'check.cpp'), '-Wl,--wrap=fsync,--wrap=fflush,--wrap=fclose',
                    '-o', str(binary)], check=True)
    subprocess.run([str(binary), str(work)], check=True)
print('PASS: screenshot flush/file/directory sync, DOS cache refresh, write/close/sync failures and standalone behavior.')
