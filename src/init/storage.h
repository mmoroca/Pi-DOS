/* Original project code; license selection remains pending. */
#ifndef PI_DOS_STORAGE_H
#define PI_DOS_STORAGE_H
#include <stdbool.h>

/* Accept only the Pi-DOS FAT32 data volume labelled PI-DOS. */
bool pi_dos_is_fat32_data(const char *device);

/* Before mounting: 1 expanded, 0 unchanged/skipped, -1 requires read-only fallback. */
int pi_dos_expand_data(const char *device);

#endif
