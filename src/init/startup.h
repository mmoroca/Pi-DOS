#ifndef ARMDOS_STARTUP_H
#define ARMDOS_STARTUP_H

#include <stdbool.h>
#include <stdio.h>

void armdos_startup(FILE *output, const char *config, const char *device,
                    bool mounted, bool writable, bool drive_c, bool drive_d);

#endif
