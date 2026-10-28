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
#include "diagnostics.h"
#include "network.h"
#include "timezone.h"
#include "config.h"
#include "video.h"
#include "mixer.h"
#include "midi.h"
#include "midi_profiles.h"
#include "power_button.h"

enum storage { NO_STORAGE, READ_ONLY, READ_WRITE };
static struct pi_dos_diagnostics diagnostics = {.kernel_fd = -1, .boot_fd = -1, .runtime_fd = -1};
static struct pi_dos_network network = {.dhcp = -1, .clock = -1};
static struct pi_dos_power_button power_button = {.fd = -1};

#define USER_CONFIG_PATH "/dosdata/c/SYSTEM/PIDOS.CFG"

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
        const char *prefix = "pi-dos.data=";
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
        dprintf(2, "Pi-DOS: PI-DOS device missing after 10 seconds: %s\n", device);
        return NO_STORAGE;
    }
    /* Reject other filesystems before changing or mounting the data partition. */
    if (!pi_dos_is_fat32_data(device)) {
        dprintf(2, "Pi-DOS: %s is not the expected FAT32 PI-DOS partition\n", device);
        return NO_STORAGE;
    }
    bool writable = pi_dos_expand_data(device) >= 0;
    unsigned long flags = MS_NODEV | MS_NOSUID | MS_NOEXEC;
    if (writable && !mount(device, "/dosdata", "vfat", flags,
                           "uid=0,gid=0,umask=0022,utf8=1,errors=remount-ro")) {
        make_dir("/dosdata/c", 0755);
        make_dir("/dosdata/c/SYSTEM", 0755);
        /* A stale request must never cause a reboot loop after interrupted power. */
        unlink("/dosdata/c/SYSTEM/CONTROL.REQ");
        return READ_WRITE;
    }
    if (writable) dprintf(2, "Pi-DOS: writable PI-DOS mount failed: %s\n", strerror(errno));
    else dprintf(2, "Pi-DOS: expansion did not complete; trying read-only PI-DOS\n");
    if (!mount(device, "/dosdata", "vfat", flags | MS_RDONLY,
               "uid=0,gid=0,umask=0022,utf8=1")) {
        dprintf(2, "Pi-DOS: using PI-DOS read-only\n");
        return READ_ONLY;
    }
    dprintf(2, "Pi-DOS: PI-DOS unavailable: %s\n", strerror(errno));
    return NO_STORAGE;
}

static void prepare_dos_folders(enum storage storage)
{
    if (storage != READ_WRITE) return;
    const char *directories[] = {"/dosdata/c/PROGRAMS", "/dosdata/c/PROGRAMS/TOOLS",
                                 "/dosdata/c/GAMES", "/dosdata/c/DOCS", "/dosdata/c/TEMP",
                                 MIDI_ASSET_ROOT, MIDI_ASSET_ROOT "/MT32", MIDI_ASSET_ROOT "/CM32L"};
    for (size_t i = 0; i < sizeof(directories) / sizeof(directories[0]); ++i)
        make_dir(directories[i], 0755);
    /* PEDIT saves its preferences inside EDIT.COM. Move the existing writable
     * copy without replacing a utility already installed in the new location. */
    struct stat status;
    if (lstat("/dosdata/c/PROGRAMS/TOOLS/EDIT.COM", &status) && errno == ENOENT &&
        !lstat("/dosdata/c/SYSTEM/EDIT.COM", &status) && S_ISREG(status.st_mode) &&
        rename("/dosdata/c/SYSTEM/EDIT.COM", "/dosdata/c/PROGRAMS/TOOLS/EDIT.COM"))
        dprintf(2, "Pi-DOS: editor migration failed: %s\n", strerror(errno));
}

static void seed_user_file(enum storage storage, const char *source, const char *destination)
{
    if (storage != READ_WRITE) return;
    struct stat status;
    if (!lstat(destination, &status) || errno != ENOENT) return;
    char temporary[PATH_MAX];
    int length = snprintf(temporary, sizeof(temporary), "%s.XXXXXX", destination);
    if (length < 0 || (size_t)length >= sizeof(temporary)) return;
    FILE *input = fopen(source, "rb");
    if (!input) {
        dprintf(2, "Pi-DOS: default %s unavailable: %s\n", source, strerror(errno));
        return;
    }
    int fd = mkstemp(temporary);
    FILE *output = fd >= 0 ? fdopen(fd, "wb") : NULL;
    if (!output) {
        if (fd >= 0) { close(fd); unlink(temporary); }
        fclose(input);
        dprintf(2, "Pi-DOS: cannot create %s: %s\n", destination, strerror(errno));
        return;
    }
    char buffer[4096];
    size_t count;
    bool valid = true;
    while ((count = fread(buffer, 1, sizeof(buffer), input))) {
        if (fwrite(buffer, 1, count, output) != count) { valid = false; break; }
    }
    if (ferror(input)) valid = false;
    fclose(input);
    if (fflush(output) || fchmod(fd, 0644) || fsync(fd)) valid = false;
    if (fclose(output)) valid = false;
    if (!valid || rename(temporary, destination)) {
        dprintf(2, "Pi-DOS: %s initialization failed\n", destination);
        unlink(temporary);
    }
}

static const char *user_config_path(enum storage storage)
{
    if (storage == NO_STORAGE) return NULL;
    struct stat status;
    if (!stat(USER_CONFIG_PATH, &status) && S_ISREG(status.st_mode) &&
        !access(USER_CONFIG_PATH, R_OK)) return USER_CONFIG_PATH;
    return NULL;
}

static bool make_config(enum storage storage, const char *user_config)
{
    FILE *input = fopen("/pi-dos/pi-dos.conf", "r");
    FILE *output = fopen("/run/pi-dos.conf", "w");
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
    fputs("Z:\\SYSTEM\\PIFLOPPY.COM\n", output);
    bool drive_c = storage != NO_STORAGE && !access("/dosdata/c", R_OK | X_OK);
    if (drive_c) {
        fprintf(output, "MOUNT C /dosdata/c -label SYSTEM%s\nC:\n", storage == READ_ONLY ? " -ro" : "");
        fputs("SET PATH=C:\\PROGRAMS\\TOOLS;C:\\SYSTEM;Z:\\;Z:\\SYSTEM;Z:\\DOS;Z:\\BIN;Z:\\TEXTUTIL;Z:\\DEBUG;Z:\\PATCHING;Z:\\4DOS\n", output);
        struct stat temporary;
        if (storage == READ_WRITE && !stat("/dosdata/c/TEMP", &temporary) &&
            S_ISDIR(temporary.st_mode) && !access("/dosdata/c/TEMP", W_OK | X_OK))
            fputs("SET TEMP=C:\\TEMP\nSET TMP=C:\\TEMP\n", output);
        fputs("CLS\n", output);
    } else {
        fputs("CLS\n", output);
    }
    char device[PATH_MAX];
    data_device(device, sizeof(device));
    pi_dos_startup(output, "/pi-dos/pi-dos.conf", user_config, device, storage != NO_STORAGE,
                   storage == READ_WRITE, drive_c);
    if (diagnostics.boot_fd >= 0 || diagnostics.runtime_fd >= 0)
        fputs("ECHO Diagnostic capture enabled; logs are in C:\\SYSTEM.\n", output);
    pi_dos_mixer(output, user_config);
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
    /* Match only the primary HDMI output by stable ALSA ID. BCM2837 boards use
     * vc4hdmi; BCM2711/2712 use vc4hdmi0. Never select HDMI1 or a USB sound card. */
    for (int attempt = 0; attempt < 50; ++attempt) {
        glob_t paths = {0};
        bool single_hdmi = false, dual_hdmi = false;
        if (!glob("/sys/class/sound/card*/id", 0, NULL, &paths)) {
            for (size_t i = 0; i < paths.gl_pathc; ++i) {
                char id[64];
                if (!read_line(paths.gl_pathv[i], id, sizeof(id))) continue;
                if (!strcmp(id, "vc4hdmi0")) dual_hdmi = true;
                if (!strcmp(id, "vc4hdmi")) single_hdmi = true;
            }
        }
        globfree(&paths);
        if (dual_hdmi || single_hdmi) {
            const char *id = dual_hdmi ? "vc4hdmi0" : "vc4hdmi";
            char device[64];
            snprintf(device, sizeof(device), "plughw:CARD=%s,DEV=0", id);
            setenv("AUDIODEV", device, 1);
            dprintf(2, "Pi-DOS: audio output fixed to primary HDMI (%s)\n", id);
            return;
        }
        delay_ms(100);
    }
    /* A missing primary HDMI device must not redirect audio to another card. */
    setenv("AUDIODEV", "plughw:CARD=vc4hdmi0,DEV=0", 1);
    dprintf(2, "Pi-DOS: primary HDMI audio device unavailable\n");
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
    pi_dos_power_button_close(&power_button);
    pi_dos_network_stop(&network);
    pi_dos_diagnostics_stop(&diagnostics);
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
        fputs("pi-dos-init must run as root PID 1; refusing to alter this system.\n", stderr);
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
    prepare_dos_folders(storage);
    seed_user_file(storage, "/pi-dos/PIDOS.CFG", USER_CONFIG_PATH);
    seed_user_file(storage, "/pi-dos/AUTOEXEC.BAT", "/dosdata/c/AUTOEXEC.BAT");
    const char *midi_assets[] = {"SC55.SF2", "GMROLAND.SF2", "MT32/MT32_CONTROL.ROM", "MT32/MT32_PCM.ROM",
                                "CM32L/CM32L_CONTROL.ROM", "CM32L/CM32L_PCM.ROM"};
    for (size_t i = 0; i < sizeof(midi_assets) / sizeof(midi_assets[0]); ++i) {
        char source[160], destination[160];
        snprintf(source, sizeof(source), "/pi-dos/midi-assets/%s", midi_assets[i]);
        snprintf(destination, sizeof(destination), MIDI_ASSET_ROOT "/%s", midi_assets[i]);
        seed_user_file(storage, source, destination);
    }
    const char *user_config = user_config_path(storage);
    /* Set the host process timezone before launching DOSBox and time services.
     * The kernel clock and hardware RTC remain in UTC. */
    pi_dos_timezone(user_config);
    bool diagnostics_enabled = pi_dos_diagnostics_enabled(user_config);
    pi_dos_diagnostics_start(&diagnostics, diagnostics_enabled, storage == READ_WRITE);
    bool capture = diagnostics_enabled && storage == READ_WRITE;
    pi_dos_network_start(&network, capture);
    /* Internal handoff of the resolved setting to the emulator's video logger. */
    setenv("PI_DOS_DIAGNOSTICS", capture ? "1" : "0", 1);
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
    /* Give USB enumeration the existing display/audio readiness waits before
     * resolving MIDI and freezing its status into the boot table. */
    if ((user_config && (!pi_dos_runtime_config(user_config, "/run/pidos-user.conf") ||
                         !pi_dos_midi_prepare(user_config, "/run/pidos-user.conf", MIDI_ASSET_ROOT))) ||
        !make_config(storage, user_config) ||
        !pi_dos_video_prepare(user_config, "/run/pidos-video.conf")) {
        dprintf(2, "Pi-DOS: runtime configuration failed\n");
        for (;;) pause();
    }
    if (mount(NULL, "/", NULL, MS_REMOUNT | MS_RDONLY, NULL))
        dprintf(2, "Pi-DOS: root read-only remount unavailable: %s (root is volatile initramfs)\n", strerror(errno));

    pid_t runtime = fork();
    if (runtime == 0) {
        setenv("PI_DOS_SUPERVISED", "1", 1);
        setenv("PI_DOS_USB_FLOPPY", "1", 1);
        pi_dos_diagnostics_redirect(&diagnostics);
        sigprocmask(SIG_UNBLOCK, &blocked, NULL);
        setsid();
        int tty = open("/dev/tty1", O_RDWR);
        if (tty >= 0) {
            ioctl(tty, TIOCSCTTY, 0);
            dup2(tty, 0); /* Keep emulator diagnostics on the serial console. */
            if (tty > 2) close(tty);
        }
        if (chdir("/pi-dos")) {
            dprintf(2, "Pi-DOS: runtime directory unavailable: %s\n", strerror(errno));
            _exit(127);
        }
        char *arguments[14] = {"pi-dos", "-defaultmapper", "-nopromptfolder",
                              "-defaultdir", "/pi-dos", "-conf", "/run/pi-dos.conf"};
        size_t argc = 7;
        /* User hardware settings override defaults, and user [autoexec]
         * commands follow system startup. The monitor profile is resolved last. */
        if (user_config) {
            arguments[argc++] = "-conf";
            arguments[argc++] = "/run/pidos-user.conf";
        }
        /* Managed monitor profiles take precedence over low-level display settings. */
        arguments[argc++] = "-conf";
        arguments[argc++] = "/run/pidos-video.conf";
        arguments[argc++] = "-fastlaunch";
        arguments[argc++] = "-nomenu";
        arguments[argc] = NULL;
        execv("/pi-dos/pi-dos", arguments);
        dprintf(2, "Pi-DOS: runtime exec failed: %s\n", strerror(errno));
        _exit(127);
    }
    if (runtime < 0) {
        dprintf(2, "Pi-DOS: runtime fork failed: %s\n", strerror(errno));
        for (;;) pause();
    }
    bool requested = false, restart = false;
    struct timespec deadline = {0};
    for (;;) {
        int status;
        pid_t child;
        while ((child = waitpid(-1, &status, WNOHANG)) > 0) {
            pi_dos_network_reaped(&network, child, status);
            if (child != runtime) continue;
            if (!requested && (!WIFEXITED(status) || WEXITSTATUS(status) != 0)) {
                dprintf(2, "Pi-DOS: runtime failed (wait status %d); keeping serial diagnostics available\n", status);
                /* No crash-restart loop. SIGINT/SIGTERM still permit safe shutdown. */
                runtime = -1;
                break;
            }
            char request[32];
            if (storage == READ_WRITE && read_line("/dosdata/c/SYSTEM/CONTROL.REQ", request, sizeof(request))) {
                if (!requested) restart = !strcmp(request, "reboot");
                unlink("/dosdata/c/SYSTEM/CONTROL.REQ");
            }
            finish(storage, restart);
        }
        pi_dos_power_button_scan(&power_button);
        struct pollfd items[] = {{signals, POLLIN, 0}, {device_events, POLLIN, 0},
                                 {diagnostics.kernel_fd, POLLIN, 0}, {power_button.fd, POLLIN, 0}};
        bool stop = false, want_restart = false;
        if (poll(items, 4, 100) > 0 && (items[0].revents & POLLIN)) {
            struct signalfd_siginfo info;
            if (read(signals, &info, sizeof(info)) == sizeof(info) &&
                (info.ssi_signo == SIGTERM || info.ssi_signo == SIGINT)) {
                stop = true;
                want_restart = info.ssi_signo == SIGINT;
            }
        }
        if ((items[3].revents & (POLLIN | POLLHUP | POLLERR | POLLNVAL)) &&
            pi_dos_power_button_read(&power_button)) {
            if (!requested) dprintf(2, "Pi-DOS: power button requested a clean poweroff\n");
            stop = true;
            want_restart = false;
        }
        if (stop && !requested) {
            restart = want_restart;
            if (runtime < 0) finish(storage, restart);
            requested = true;
            clock_gettime(CLOCK_MONOTONIC, &deadline);
            deadline.tv_sec += 10;
            /* The runtime uses this volatile marker to bypass quit dialogs. */
            int marker = open("/run/pi-dos-stop", O_WRONLY | O_CREAT | O_CLOEXEC, 0600);
            if (marker >= 0) close(marker);
            else dprintf(2, "Pi-DOS: shutdown marker failed: %s\n", strerror(errno));
            kill(runtime, SIGTERM);
        }
        if (items[1].revents & POLLIN) forward_device_events(device_events);
        pi_dos_diagnostics_poll(&diagnostics, runtime);
        struct timespec now;
        clock_gettime(CLOCK_MONOTONIC, &now);
        if (requested && (now.tv_sec > deadline.tv_sec ||
            (now.tv_sec == deadline.tv_sec && now.tv_nsec >= deadline.tv_nsec)))
            kill(runtime, SIGKILL);
    }
}
