#define _GNU_SOURCE
/* Original project code; license selection remains pending. */
#include "diagnostics.h"
#include <errno.h>
#include <fcntl.h>
#include <glob.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define BOOT_LOG "/dosdata/c/SYSTEM/BOOT.LOG"
#define RUNTIME_LOG "/dosdata/c/SYSTEM/RUNTIME.LOG"
#define BOOT_LOG_LIMIT (4 * 1024 * 1024)

bool pi_dos_diagnostics_enabled(void)
{
    char buffer[4096];
    FILE *input = fopen("/proc/cmdline", "r");
    if (!input) return false;
    bool valid = fgets(buffer, sizeof(buffer), input) != NULL;
    fclose(input);
    if (!valid) return false;
    bool result = false;
    char *save = NULL;
    for (char *word = strtok_r(buffer, " \t\r\n", &save); word;
         word = strtok_r(NULL, " \t\r\n", &save)) {
        if (!strcmp(word, "pi-dos.diagnostics=1")) result = true;
        if (!strcmp(word, "pi-dos.diagnostics=0")) result = false;
    }
    return result;
}

static void append(struct pi_dos_diagnostics *log, const char *data, size_t length)
{
    if (log->boot_fd < 0 || log->bytes >= BOOT_LOG_LIMIT) return;
    if (length > BOOT_LOG_LIMIT - log->bytes) length = BOOT_LOG_LIMIT - log->bytes;
    while (length) {
        ssize_t count = write(log->boot_fd, data, length);
        if (count < 0 && errno == EINTR) continue;
        if (count <= 0) {
            dprintf(2, "Pi-DOS: diagnostic boot log write failed: %s\n", strerror(errno));
            close(log->boot_fd);
            log->boot_fd = -1;
            break;
        }
        data += count;
        length -= (size_t)count;
        log->bytes += (size_t)count;
        log->dirty = true;
    }
    if (log->boot_fd < 0 || log->bytes >= BOOT_LOG_LIMIT) {
        if (log->kernel_fd >= 0) close(log->kernel_fd);
        log->kernel_fd = -1;
        if (log->boot_fd >= 0)
            dprintf(log->boot_fd, "\n[Pi-DOS: BOOT.LOG reached its 4 MiB limit; capture stopped]\n");
    }
}

static void drain_kernel(struct pi_dos_diagnostics *log)
{
    for (int record = 0; record < 256 && log->kernel_fd >= 0; ++record) {
        char buffer[16384];
        ssize_t count = read(log->kernel_fd, buffer, sizeof(buffer));
        if (count > 0) {
            append(log, buffer, (size_t)count);
        } else if (count < 0 && errno == EPIPE) {
            const char lost[] = "\n[Pi-DOS: kernel ring overflow; some records were lost]\n";
            append(log, lost, sizeof(lost) - 1);
        } else if (count < 0 && errno == EINTR) {
            continue;
        } else if (count < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
            break;
        } else {
            if (count < 0)
                dprintf(2, "Pi-DOS: kernel log read failed: %s\n", strerror(errno));
            close(log->kernel_fd);
            log->kernel_fd = -1;
        }
    }
}

static void copy_text(FILE *output, const char *path)
{
    FILE *input = fopen(path, "r");
    if (!input) {
        fprintf(output, "%s unavailable: %s\n", path, strerror(errno));
        return;
    }
    char buffer[4096];
    size_t count, total = 0;
    while (total < 65536 && (count = fread(buffer, 1, sizeof(buffer), input))) {
        fwrite(buffer, 1, count, output);
        total += count;
    }
    fclose(input);
}

static void snapshot(struct pi_dos_diagnostics *log, pid_t runtime, time_t now)
{
    char *text = NULL;
    size_t length = 0;
    FILE *output = open_memstream(&text, &length);
    if (!output) return;
    fputs("Input devices (/proc/bus/input/devices):\n", output);
    copy_text(output, "/proc/bus/input/devices");
    const char *variables[] = {"SDL_VIDEODRIVER", "SDL_KMSDRM_DEVICE_INDEX", "AUDIODEV"};
    for (size_t i = 0; i < sizeof(variables) / sizeof(variables[0]); ++i) {
        const char *value = getenv(variables[i]);
        fprintf(output, "%s=%s\n", variables[i], value ? value : "(unset)");
    }
    fprintf(output, "Runtime PID: %ld\nOpened evdev handles:\n", (long)runtime);
    bool found = false;
    if (runtime > 0) {
        char pattern[64];
        snprintf(pattern, sizeof(pattern), "/proc/%ld/fd/*", (long)runtime);
        glob_t paths = {0};
        if (!glob(pattern, 0, NULL, &paths)) {
            for (size_t i = 0; i < paths.gl_pathc; ++i) {
                char target[PATH_MAX];
                ssize_t count = readlink(paths.gl_pathv[i], target, sizeof(target) - 1);
                if (count < 0) continue;
                target[count] = 0;
                if (!strncmp(target, "/dev/input/", 11)) {
                    fprintf(output, "%s -> %s\n", paths.gl_pathv[i], target);
                    found = true;
                }
            }
        }
        globfree(&paths);
    }
    if (!found) fputs("(none)\n", output);
    if (fclose(output)) { free(text); return; }
    uint64_t hash = UINT64_C(14695981039346656037);
    for (size_t i = 0; i < length; ++i) hash = (hash ^ (unsigned char)text[i]) * UINT64_C(1099511628211);
    if (!log->have_snapshot || hash != log->snapshot_hash) {
        char heading[96];
        int count = snprintf(heading, sizeof(heading), "\n[Pi-DOS input snapshot at %ld seconds]\n", (long)now);
        if (count > 0) append(log, heading, (size_t)count);
        append(log, text, length);
        log->snapshot_hash = hash;
        log->have_snapshot = true;
    }
    free(text);
}

void pi_dos_diagnostics_start(struct pi_dos_diagnostics *log, bool writable)
{
    *log = (struct pi_dos_diagnostics){.kernel_fd = -1, .boot_fd = -1, .runtime_fd = -1};
    if (!pi_dos_diagnostics_enabled()) return;
    if (!writable) {
        dprintf(2, "Pi-DOS: diagnostics require writable C:; using serial output only\n");
        return;
    }
    log->boot_fd = open(BOOT_LOG, O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC | O_NOFOLLOW, 0644);
    if (log->boot_fd >= 0) {
        const char heading[] = "Pi-DOS diagnostic boot log\nKernel records: priority,sequence,microseconds,flags;message\n";
        append(log, heading, sizeof(heading) - 1);
        if (log->boot_fd >= 0)
            log->kernel_fd = open("/dev/kmsg", O_RDONLY | O_NONBLOCK | O_CLOEXEC);
        if (log->boot_fd >= 0 && log->kernel_fd < 0) {
            char error[256];
            int count = snprintf(error, sizeof(error), "Kernel log unavailable: %s\n", strerror(errno));
            if (count > 0) append(log, error, (size_t)count);
        }
        drain_kernel(log);
        if (log->boot_fd >= 0 && log->dirty) { fsync(log->boot_fd); log->dirty = false; }
    } else {
        dprintf(2, "Pi-DOS: cannot create BOOT.LOG: %s\n", strerror(errno));
    }
    log->runtime_fd = open(RUNTIME_LOG, O_WRONLY | O_CREAT | O_TRUNC | O_APPEND |
                           O_CLOEXEC | O_NOFOLLOW | O_DSYNC, 0644);
    if (log->runtime_fd >= 0)
        dprintf(log->runtime_fd, "Pi-DOS diagnostic runtime output\n");
    else
        dprintf(2, "Pi-DOS: cannot create RUNTIME.LOG: %s\n", strerror(errno));
}

void pi_dos_diagnostics_poll(struct pi_dos_diagnostics *log, pid_t runtime)
{
    if (log->boot_fd < 0) return;
    drain_kernel(log);
    if (log->boot_fd < 0) return;
    struct timespec now;
    if (clock_gettime(CLOCK_MONOTONIC, &now)) return;
    if (log->bytes < BOOT_LOG_LIMIT && now.tv_sec >= log->next_snapshot) {
        snapshot(log, runtime, now.tv_sec);
        log->next_snapshot = now.tv_sec + 5;
    }
    if (log->dirty && now.tv_sec >= log->next_sync) {
        if (fsync(log->boot_fd))
            dprintf(2, "Pi-DOS: diagnostic boot log sync failed: %s\n", strerror(errno));
        log->dirty = false;
        log->next_sync = now.tv_sec + 1;
    }
}

void pi_dos_diagnostics_redirect(const struct pi_dos_diagnostics *log)
{
    if (log->runtime_fd >= 0 &&
        (dup2(log->runtime_fd, STDOUT_FILENO) < 0 || dup2(log->runtime_fd, STDERR_FILENO) < 0))
        dprintf(2, "Pi-DOS: runtime log redirection failed: %s\n", strerror(errno));
}

void pi_dos_diagnostics_stop(struct pi_dos_diagnostics *log)
{
    drain_kernel(log);
    if (log->boot_fd >= 0) { fsync(log->boot_fd); close(log->boot_fd); }
    if (log->runtime_fd >= 0) { fsync(log->runtime_fd); close(log->runtime_fd); }
    if (log->kernel_fd >= 0) close(log->kernel_fd);
    log->boot_fd = log->runtime_fd = log->kernel_fd = -1;
}
