#!/usr/bin/env python3
"""Feed evdev records to the real listener without using hardware or poweroff."""
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]
with tempfile.TemporaryDirectory(prefix='pi-dos-button.') as temporary:
    work = Path(temporary)
    source = work / 'button.c'
    source.write_text(r'''
#define _GNU_SOURCE
#include <assert.h>
#include <stdarg.h>
#include <sys/ioctl.h>
static int test_ioctl(int fd, unsigned long request, ...);
#define ioctl test_ioctl
#include "power_button.c"
#undef ioctl

static const char *name = "pwr_button";
static int bus = BUS_HOST, supported = 1, held = 0, key_error = 0;
static int test_ioctl(int fd, unsigned long request, ...) {
    (void)fd;
    va_list args; va_start(args, request); void *value = va_arg(args, void *); va_end(args);
    if (request == EVIOCGNAME(80)) { strcpy(value, name); return (int)strlen(name) + 1; }
    if (request == EVIOCGID) { ((struct input_id *)value)->bustype = bus; return 0; }
    if (request == EVIOCGBIT(EV_KEY, KEY_BYTES) || request == EVIOCGKEY(KEY_BYTES)) {
        if (request == EVIOCGKEY(KEY_BYTES) && key_error) return -1;
        memset(value, 0, KEY_BYTES);
        if (request == EVIOCGKEY(KEY_BYTES) ? held : supported)
            ((unsigned char *)value)[KEY_POWER / 8] |= 1U << (KEY_POWER % 8);
        return 0;
    }
    assert(0); return -1;
}
static void send_event(int fd, unsigned short type, unsigned short code, int value) {
    struct input_event event = {.type = type, .code = code, .value = value};
    assert(write(fd, &event, sizeof(event)) == sizeof(event));
}
int main(void) {
    bool down = false;
    assert(onboard_button(7, &down) && !down);
    bus = BUS_USB; assert(!onboard_button(7, &down)); bus = BUS_HOST;
    name = "USB keyboard"; assert(!onboard_button(7, &down)); name = "pwr_button";
    supported = 0; assert(!onboard_button(7, &down)); supported = 1;
    key_error = 1; assert(!onboard_button(7, &down)); key_error = 0;
    held = 1; assert(onboard_button(7, &down) && down);

    int pipefd[2]; assert(!pipe2(pipefd, O_NONBLOCK | O_CLOEXEC));
    struct pi_dos_power_button button = {.fd = pipefd[0], .down = down};
    pi_dos_power_button_scan(&button); // An open listener is not replaced.
    send_event(pipefd[1], EV_KEY, KEY_POWER, 1);
    assert(!pi_dos_power_button_read(&button)); // held at startup
    send_event(pipefd[1], EV_KEY, KEY_POWER, 0);
    send_event(pipefd[1], EV_KEY, KEY_POWER, 1);
    assert(pi_dos_power_button_read(&button));
    send_event(pipefd[1], EV_KEY, KEY_POWER, 2);
    send_event(pipefd[1], EV_KEY, KEY_POWER, 1);
    send_event(pipefd[1], EV_KEY, KEY_A, 1);
    assert(!pi_dos_power_button_read(&button));
    send_event(pipefd[1], EV_KEY, KEY_POWER, 0);
    assert(!pi_dos_power_button_read(&button));

    send_event(pipefd[1], EV_SYN, SYN_DROPPED, 0);
    send_event(pipefd[1], EV_KEY, KEY_POWER, 1);
    send_event(pipefd[1], EV_SYN, SYN_REPORT, 0);
    assert(!pi_dos_power_button_read(&button) && button.down && !button.dropped);
    send_event(pipefd[1], EV_KEY, KEY_POWER, 0);
    send_event(pipefd[1], EV_KEY, KEY_POWER, 1);
    assert(pi_dos_power_button_read(&button));
    close(pipefd[1]); assert(!pi_dos_power_button_read(&button) && button.fd == -1);

    assert(!pipe2(pipefd, O_NONBLOCK | O_CLOEXEC)); button.fd = pipefd[0];
    key_error = 1;
    send_event(pipefd[1], EV_SYN, SYN_DROPPED, 0);
    send_event(pipefd[1], EV_SYN, SYN_REPORT, 0);
    assert(!pi_dos_power_button_read(&button) && button.fd == -1);
    close(pipefd[1]); pi_dos_power_button_close(&button);
}
''')
    binary = work / 'button'
    subprocess.run(['cc', '-std=c11', '-Wall', '-Wextra', '-Werror',
                    '-I'+str(ROOT/'src/init'), str(source), '-o', str(binary)], check=True)
    subprocess.run([str(binary)], check=True)
print('PASS: onboard device filtering, press/release, held boot button, repeats, event-loss recovery and disconnect; no shutdown performed.')
