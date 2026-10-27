#define _GNU_SOURCE
#include "power_button.h"
#include <errno.h>
#include <fcntl.h>
#include <glob.h>
#include <linux/input.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>

#define KEY_BYTES ((KEY_MAX + 8) / 8)
static bool power_down(int fd, bool *down)
{
    unsigned char keys[KEY_BYTES] = {0};
    if (ioctl(fd, EVIOCGKEY(sizeof(keys)), keys) < 0) return false;
    *down = (keys[KEY_POWER / 8] & (1U << (KEY_POWER % 8))) != 0;
    return true;
}

static bool onboard_button(int fd, bool *down)
{
    char name[80] = {0};
    struct input_id id = {0};
    unsigned char keys[KEY_BYTES] = {0};
    if (ioctl(fd, EVIOCGNAME(sizeof(name)), name) < 0 ||
        ioctl(fd, EVIOCGID, &id) < 0 || id.bustype != BUS_HOST) return false;
    name[sizeof(name) - 1] = 0;
    if (strcmp(name, "pwr_button") ||
        ioctl(fd, EVIOCGBIT(EV_KEY, sizeof(keys)), keys) < 0 ||
        !(keys[KEY_POWER / 8] & (1U << (KEY_POWER % 8)))) return false;
    return power_down(fd, down);
}

void pi_dos_power_button_close(struct pi_dos_power_button *button)
{
    if (button->fd >= 0) close(button->fd);
    button->fd = -1;
    button->down = button->dropped = false;
}

void pi_dos_power_button_scan(struct pi_dos_power_button *button)
{
    if (button->fd >= 0) return;
    struct timespec now;
    if (clock_gettime(CLOCK_MONOTONIC, &now) || now.tv_sec < button->next_scan) return;
    button->next_scan = now.tv_sec + 5;
    glob_t paths = {0};
    if (!glob("/dev/input/event*", 0, NULL, &paths)) {
        for (size_t i = 0; i < paths.gl_pathc; ++i) {
            int fd = open(paths.gl_pathv[i], O_RDONLY | O_NONBLOCK | O_CLOEXEC);
            if (fd < 0) continue;
            bool down;
            if (onboard_button(fd, &down)) {
                button->fd = fd;
                button->down = down; /* A button held during boot must first be released. */
                button->dropped = false;
                dprintf(2, "Pi-DOS: power button ready on %s\n", paths.gl_pathv[i]);
                break;
            }
            close(fd);
        }
    }
    globfree(&paths);
}

bool pi_dos_power_button_read(struct pi_dos_power_button *button)
{
    bool pressed = false;
    for (int pending = 0; button->fd >= 0 && pending < 64; ++pending) {
        struct input_event event;
        ssize_t count = read(button->fd, &event, sizeof(event));
        if (count < 0 && errno == EINTR) continue;
        if (count < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) break;
        if (count != sizeof(event)) {
            pi_dos_power_button_close(button);
            break;
        }
        if (event.type == EV_SYN && event.code == SYN_DROPPED) {
            button->dropped = true;
        } else if (button->dropped) {
            if (event.type == EV_SYN && event.code == SYN_REPORT) {
                button->dropped = false;
                /* Resynchronise without inventing a press from lost events. */
                if (!power_down(button->fd, &button->down)) pi_dos_power_button_close(button);
            }
        } else if (event.type == EV_KEY && event.code == KEY_POWER) {
            if (event.value == 1) {
                if (!button->down) pressed = true;
                button->down = true;
            } else if (event.value == 0) button->down = false;
        }
    }
    return pressed;
}
