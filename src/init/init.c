#define _GNU_SOURCE
/* Pi-DOS PID 1. Original project code; license selection remains pending. */
#include <errno.h>
#include <fcntl.h>
#include <glob.h>
#include <limits.h>
#include <linux/netlink.h>
#include <poll.h>
#include <signal.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mount.h>
#include <sys/reboot.h>
#include <sys/signalfd.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>
#include "storage.h"
#include "startup.h"

enum storage { NO_STORAGE, READ_ONLY, READ_WRITE };

static int open_device_events(void)
{
    /* devtmpfs creates nodes; libudev-zero still needs userspace notifications.
     * Subscribe before starting SDL so asynchronous USB enumeration is covered. */
    int fd = socket(AF_NETLINK, SOCK_DGRAM | SOCK_NONBLOCK | SOCK_CLOEXEC,
                    NETLINK_KOBJECT_UEVENT);
    struct sockaddr_nl address = {.nl_family = AF_NETLINK, .nl_groups = 1};
    if (fd < 0) {
        dprintf(2, "Pi-DOS: device event socket failed: %s\n", strerror(errno));
        return -1;
    }
    int size = 256 * 1024;
    setsockopt(fd, SOL_SOCKET, SO_RCVBUF, &size, sizeof(size));
    if (bind(fd, (struct sockaddr *)&address, sizeof(address))) {
        dprintf(2, "Pi-DOS: device event subscription failed: %s\n", strerror(errno));
        close(fd);
        return -1;
    }
    return fd;
}

static void forward_device_events(int fd)
{
    /* Only forward kernel input/sound messages, without modifying their fields.
     * No udev rules, permission changes or symlinks are needed by this root-only
     * devtmpfs appliance. libudev-zero classifies devices from sysfs itself. */
    for (int pending = 0; pending < 64; ++pending) {
        char buffer[8192];
        struct sockaddr_nl source = {0};
        struct iovec vector = {buffer, sizeof(buffer)};
        struct msghdr message = {.msg_name = &source, .msg_namelen = sizeof(source),
                                 .msg_iov = &vector, .msg_iovlen = 1};
        ssize_t count = recvmsg(fd, &message, 0);
        if (count < 0) {
            if (errno != EAGAIN && errno != EINTR)
                dprintf(2, "Pi-DOS: device event receive failed: %s\n", strerror(errno));
            return;
        }
        if (!count || (message.msg_flags & MSG_TRUNC) || source.nl_pid != 0 ||
            source.nl_groups != 1 || buffer[count - 1] != 0) continue;
        bool relevant = false;
        for (size_t at = 0; at < (size_t)count; at += strlen(buffer + at) + 1) {
            if (!strcmp(buffer + at, "SUBSYSTEM=input") ||
                !strcmp(buffer + at, "SUBSYSTEM=sound")) relevant = true;
        }
        if (!relevant) continue;
        struct sockaddr_nl destination = {.nl_family = AF_NETLINK, .nl_groups = 4};
        if (sendto(fd, buffer, (size_t)count, 0,
                   (struct sockaddr *)&destination, sizeof(destination)) < 0 &&
            errno != ESRCH) /* No listeners yet: SDL's initial scan covers these. */
            dprintf(2, "Pi-DOS: device notification failed: %s\n", strerror(errno));
    }
}

static void delay_ms(long ms)
{
    struct timespec delay = {ms / 1000, ms % 1000 * 1000000};
    while (nanosleep(&delay, &delay) && errno == EINTR) {}
}

static void make_dir(const char *path, mode_t mode)
{
    if (mkdir(path, mode) && errno != EEXIST) {
        dprintf(2, "Pi-DOS: mkdir %s: %s\n", path, strerror(errno));
    }
}

static void mount_required(const char *source, const char *target,
                           const char *type, unsigned long flags, const char *options)
{
    make_dir(target, 0755);
    if (mount(source, target, type, flags, options) && errno != EBUSY) {
        dprintf(2, "Pi-DOS: mount %s failed: %s\n", target, strerror(errno));
        /* PID 1 must never exit or fall through to an absent shell. */
        for (;;) pause();
    }
}

static bool read_line(const char *path, char *buffer, size_t size)
{
    FILE *input = fopen(path, "r");
    if (!input) return false;
    bool success = fgets(buffer, (int)size, input) != NULL;
    fclose(input);
    if (success) buffer[strcspn(buffer, "\r\n")] = 0;
    return success;
}

static void data_device(char *device, size_t size)
{
    snprintf(device, size, "/dev/mmcblk0p2");
    char line[4096];
    if (!read_line("/proc/cmdline", line, sizeof(line))) return;
    char *save = NULL;
    for (char *token = strtok_r(line, " ", &save); token; token = strtok_r(NULL, " ", &save)) {
        const char *prefix = "armdos.data=";
        if (!strncmp(token, prefix, strlen(prefix)) &&
            !strncmp(token + strlen(prefix), "/dev/", 5)) {
            snprintf(device, size, "%s", token + strlen(prefix));
        }
    }
}

static enum storage mount_data(void)
{
    char device[PATH_MAX];
    data_device(device, sizeof(device));
    struct stat status;
    bool ready = false;
    for (int attempt = 0; attempt < 100; ++attempt) {
        if (!stat(device, &status) && S_ISBLK(status.st_mode)) {
            ready = true;
            break;
        }
        delay_ms(100);
    }
    if (!ready) {
        dprintf(2, "Pi-DOS: DOSDATA device missing after 10 seconds: %s\n", device);
        return NO_STORAGE;
    }
    /* Reject other filesystems before changing or mounting the data partition. */
    if (!armdos_is_fat32_data(device)) {
        dprintf(2, "Pi-DOS: %s is not the expected FAT32 DOSDATA partition\n", device);
        return NO_STORAGE;
    }
    bool writable = armdos_expand_data(device) >= 0;
    unsigned long flags = MS_NODEV | MS_NOSUID | MS_NOEXEC;
    if (writable && !mount(device, "/dosdata", "vfat", flags,
                           "uid=0,gid=0,umask=0022,utf8=1,errors=remount-ro")) {
        make_dir("/dosdata/c", 0755);
        make_dir("/dosdata/c/SYSTEM", 0755);
        make_dir("/dosdata/d", 0755);
        /* A stale request must never cause a reboot loop after interrupted power. */
        unlink("/dosdata/c/SYSTEM/CONTROL.REQ");
        return READ_WRITE;
    }
    if (writable) dprintf(2, "Pi-DOS: writable DOSDATA mount failed: %s\n", strerror(errno));
    else dprintf(2, "Pi-DOS: expansion did not complete; trying read-only DOSDATA\n");
    if (!mount(device, "/dosdata", "vfat", flags | MS_RDONLY,
               "uid=0,gid=0,umask=0022,utf8=1")) {
        dprintf(2, "Pi-DOS: using DOSDATA read-only\n");
        return READ_ONLY;
    }
    dprintf(2, "Pi-DOS: DOSDATA unavailable: %s\n", strerror(errno));
    return NO_STORAGE;
}

static bool make_config(enum storage storage)
{
    FILE *input = fopen("/armdos/armdos.conf", "r");
    FILE *output = fopen("/run/armdos.conf", "w");
    if (!input || !output) {
        if (input) fclose(input);
        if (output) fclose(output);
        return false;
    }
    char buffer[4096];
    while (fgets(buffer, sizeof(buffer), input)) fputs(buffer, output);
    bool valid = !ferror(input);
    fclose(input);
    fputs("\n[autoexec]\n@ECHO OFF\n", output);
    bool drive_c = storage != NO_STORAGE && !access("/dosdata/c", R_OK | X_OK);
    bool drive_d = drive_c && !access("/dosdata/d", R_OK | X_OK);
    if (drive_c) {
        fprintf(output, "MOUNT C /dosdata/c%s\nC:\n", storage == READ_ONLY ? " -ro" : "");
        if (drive_d)
            fprintf(output, "MOUNT D /dosdata/d%s\n", storage == READ_ONLY ? " -ro" : "");
        fputs("SET PATH=C:\\SYSTEM;Z:\\;Z:\\SYSTEM;Z:\\DOS;Z:\\BIN;Z:\\TEXTUTIL;Z:\\DEBUG;Z:\\PATCHING;Z:\\4DOS\nCLS\n", output);
    } else {
        fputs("CLS\n", output);
    }
    char device[PATH_MAX];
    data_device(device, sizeof(device));
    armdos_startup(output, "/armdos/armdos.conf", device, storage != NO_STORAGE,
                   storage == READ_WRITE, drive_c, drive_d);
    if (drive_c) {
        if (storage == READ_ONLY)
            fputs("ECHO WARNING: drive C: is read-only; storage requires recovery.\n", output);
        fputs("IF EXIST C:\\AUTOEXEC.BAT CALL C:\\AUTOEXEC.BAT\n", output);
    } else {
        fputs("ECHO Pi-DOS: persistent DOS storage is unavailable.\n"
              "ECHO Recovery prompt on Z:; C: has not been created in RAM.\n"
              "ECHO Type EXIT to shut down.\n", output);
    }
    if (ferror(output)) valid = false;
    if (fclose(output)) valid = false;
    return valid;
}

static void select_audio(void)
{
    /* HDMI0 (HDMI-A-1): use the stable ALSA ID, not its enumeration index.
     * Do not route to HDMI1 or the default card when ELD is unavailable. */
    setenv("AUDIODEV", "plughw:CARD=vc4hdmi0,DEV=0", 1);
    dprintf(2, "Pi-DOS: audio output fixed to HDMI0 (vc4hdmi0)\n");
}

static void select_video(void)
{
    /* Pi 5 exposes a render GPU card as well as its display card. */
    for (int attempt = 0; attempt < 50; ++attempt) {
        glob_t paths = {0};
        if (!glob("/sys/class/drm/card*-HDMI-*/status", 0, NULL, &paths)) {
            for (size_t i = 0; i < paths.gl_pathc; ++i) {
                char state[32], index[16], node[64];
                unsigned int card;
                if (read_line(paths.gl_pathv[i], state, sizeof(state)) &&
                    !strcmp(state, "connected") &&
                    sscanf(paths.gl_pathv[i], "/sys/class/drm/card%u-", &card) == 1) {
                    snprintf(node, sizeof(node), "/dev/dri/card%u", card);
                    if (access(node, R_OK | W_OK)) continue;
                    snprintf(index, sizeof(index), "%u", card);
                    setenv("SDL_KMSDRM_DEVICE_INDEX", index, 1);
                    globfree(&paths);
                    return;
                }
            }
        }
        globfree(&paths);
        delay_ms(100);
    }
    dprintf(2, "Pi-DOS: no connected HDMI card after 5 seconds; trying SDL discovery\n");
}

static void finish(enum storage storage, bool restart)
{
    dprintf(2, "Pi-DOS: synchronizing storage before %s\n", restart ? "reboot" : "poweroff");
    sync();
    if (storage != NO_STORAGE && umount("/dosdata")) {
        dprintf(2, "Pi-DOS: data unmount failed: %s; attempting read-only remount\n", strerror(errno));
        mount(NULL, "/dosdata", NULL, MS_REMOUNT | MS_RDONLY | MS_NODEV | MS_NOSUID | MS_NOEXEC, NULL);
        sync();
    }
    reboot(restart ? RB_AUTOBOOT : RB_POWER_OFF);
    dprintf(2, "Pi-DOS: system power control returned: %s\n", strerror(errno));
    for (;;) pause();
}

int main(void)
{
    if (getpid() != 1 || geteuid() != 0) {
        fputs("armdos-init must run as root PID 1; refusing to alter this system.\n", stderr);
        return 1;
    }
    mount_required("devtmpfs", "/dev", "devtmpfs", MS_NOSUID, "mode=0755");
    int console = open("/dev/console", O_RDWR | O_NOCTTY);
    if (console >= 0) {
        for (int i = 0; i < 3; ++i) dup2(console, i);
        if (console > 2) close(console);
    }
    mount_required("proc", "/proc", "proc", MS_NOSUID | MS_NODEV | MS_NOEXEC, NULL);
    mount_required("sysfs", "/sys", "sysfs", MS_NOSUID | MS_NODEV | MS_NOEXEC, NULL);
    mount_required("tmpfs", "/run", "tmpfs", MS_NOSUID | MS_NODEV, "mode=0755,size=16m");
    mount_required("tmpfs", "/tmp", "tmpfs", MS_NOSUID | MS_NODEV, "mode=1777,size=64m");
    make_dir("/dev/pts", 0755);
    mount_required("devpts", "/dev/pts", "devpts", MS_NOSUID | MS_NOEXEC, "mode=0620");
    make_dir("/dosdata", 0755);
    reboot(RB_DISABLE_CAD); /* Ctrl+Alt+Del is handled through PID 1, never a hard reset. */
    int device_events = open_device_events();

    sigset_t blocked;
    sigemptyset(&blocked);
    sigaddset(&blocked, SIGCHLD);
    sigaddset(&blocked, SIGTERM);
    sigaddset(&blocked, SIGINT);
    sigprocmask(SIG_BLOCK, &blocked, NULL);
    int signals = signalfd(-1, &blocked, SFD_CLOEXEC);
    if (signals < 0) {
        dprintf(2, "Pi-DOS: signalfd failed: %s\n", strerror(errno));
        for (;;) pause();
    }
    enum storage storage = mount_data();
    if (!make_config(storage)) {
        dprintf(2, "Pi-DOS: runtime configuration failed\n");
        for (;;) pause();
    }
    setenv("HOME", "/run", 1);
    setenv("XDG_CONFIG_HOME", "/run", 1);
    setenv("XDG_CACHE_HOME", "/run", 1);
    setenv("SDL_VIDEODRIVER", "kmsdrm", 1);
    /* DOS renders its own pointer; never expose SDL's hardware cursor. */
    setenv("SDL_KMSDRM_HIDE_CURSOR", "1", 1);
    setenv("SDL_AUDIODRIVER", "alsa", 1);
    setenv("SDL_RENDER_DRIVER", "opengles2", 1);
    setenv("SDL_FRAMEBUFFER_ACCELERATION", "opengles2", 1);
    /* Synchronize surface presentation to the physical display's vblank. */
    setenv("SDL_RENDER_VSYNC", "1", 1);
    select_video();
    select_audio();
    if (mount(NULL, "/", NULL, MS_REMOUNT | MS_RDONLY, NULL))
        dprintf(2, "Pi-DOS: root read-only remount unavailable: %s (root is volatile initramfs)\n", strerror(errno));

    pid_t runtime = fork();
    if (runtime == 0) {
        sigprocmask(SIG_UNBLOCK, &blocked, NULL);
        setsid();
        int tty = open("/dev/tty1", O_RDWR);
        if (tty >= 0) {
            ioctl(tty, TIOCSCTTY, 0);
            dup2(tty, 0); /* Keep emulator diagnostics on the serial console. */
            if (tty > 2) close(tty);
        }
        if (chdir("/armdos")) {
            dprintf(2, "Pi-DOS: runtime directory unavailable: %s\n", strerror(errno));
            _exit(127);
        }
        execl("/armdos/armdos", "armdos", "-defaultmapper",
              "-nopromptfolder", "-defaultdir", "/armdos", "-conf", "/run/armdos.conf",
              "-fastlaunch", "-nomenu", (char *)NULL);
        dprintf(2, "Pi-DOS: runtime exec failed: %s\n", strerror(errno));
        _exit(127);
    }
    if (runtime < 0) {
        dprintf(2, "Pi-DOS: runtime fork failed: %s\n", strerror(errno));
        for (;;) pause();
    }
    bool requested = false, restart = false;
    time_t deadline = 0;
    for (;;) {
        int status;
        pid_t child;
        while ((child = waitpid(-1, &status, WNOHANG)) > 0) {
            if (child != runtime) continue;
            if (!requested && (!WIFEXITED(status) || WEXITSTATUS(status) != 0)) {
                dprintf(2, "Pi-DOS: runtime failed (wait status %d); keeping serial diagnostics available\n", status);
                /* No crash-restart loop. SIGINT/SIGTERM still permit safe shutdown. */
                runtime = -1;
                break;
            }
            char request[32];
            if (storage == READ_WRITE && read_line("/dosdata/c/SYSTEM/CONTROL.REQ", request, sizeof(request))) {
                restart = !strcmp(request, "reboot");
                unlink("/dosdata/c/SYSTEM/CONTROL.REQ");
            }
            finish(storage, restart);
        }
        struct pollfd items[] = {{signals, POLLIN, 0}, {device_events, POLLIN, 0}};
        if (poll(items, 2, 100) > 0 && (items[0].revents & POLLIN)) {
            struct signalfd_siginfo info;
            if (read(signals, &info, sizeof(info)) == sizeof(info) &&
                (info.ssi_signo == SIGTERM || info.ssi_signo == SIGINT)) {
                restart = info.ssi_signo == SIGINT;
                if (runtime < 0) finish(storage, restart);
                if (!requested) {
                    requested = true;
                    deadline = time(NULL) + 10;
                    kill(runtime, SIGTERM);
                }
            }
        }
        if (items[1].revents & POLLIN) forward_device_events(device_events);
        if (requested && time(NULL) >= deadline) kill(runtime, SIGKILL);
    }
}
