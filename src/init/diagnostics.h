#ifndef ARMDOS_DIAGNOSTICS_H
#define ARMDOS_DIAGNOSTICS_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <sys/types.h>
#include <time.h>

struct armdos_diagnostics {
    int kernel_fd, boot_fd, runtime_fd;
    size_t bytes;
    time_t next_sync, next_snapshot;
    uint64_t snapshot_hash;
    bool have_snapshot, dirty;
};

void armdos_diagnostics_start(struct armdos_diagnostics *log, bool writable);
void armdos_diagnostics_poll(struct armdos_diagnostics *log, pid_t runtime);
void armdos_diagnostics_redirect(const struct armdos_diagnostics *log);
void armdos_diagnostics_stop(struct armdos_diagnostics *log);

#endif
