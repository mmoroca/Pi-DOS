#ifndef PI_DOS_DIAGNOSTICS_H
#define PI_DOS_DIAGNOSTICS_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <sys/types.h>
#include <time.h>

struct pi_dos_diagnostics {
    int kernel_fd, boot_fd, runtime_fd;
    size_t bytes;
    time_t next_sync, next_snapshot;
    uint64_t snapshot_hash;
    bool have_snapshot, dirty;
};

bool pi_dos_diagnostics_enabled(const char *config);
void pi_dos_diagnostics_start(struct pi_dos_diagnostics *log, bool enabled, bool writable);
void pi_dos_diagnostics_poll(struct pi_dos_diagnostics *log, pid_t runtime);
void pi_dos_diagnostics_redirect(const struct pi_dos_diagnostics *log);
void pi_dos_diagnostics_stop(struct pi_dos_diagnostics *log);

#endif
