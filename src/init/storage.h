/* Original project code; license selection remains pending. */
#ifndef ARMDOS_STORAGE_H
#define ARMDOS_STORAGE_H
#include <stdbool.h>

/* Accept only the Pi-DOS FAT32 data volume labelled PI-DOS. */
bool armdos_is_fat32_data(const char *device);

/* Before mounting: 1 expanded, 0 unchanged/skipped, -1 requires read-only fallback. */
int armdos_expand_data(const char *device);

#endif
