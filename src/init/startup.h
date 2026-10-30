#ifndef PI_DOS_STARTUP_H
#define PI_DOS_STARTUP_H

#include <stdbool.h>
#include <stdio.h>

void pi_dos_startup(FILE *output, const char *config, const char *user_config, const char *device,
                    bool mounted, bool writable, bool drive_c);
void pi_dos_first_boot(FILE *output, const char *user_config, bool writable, bool drive_c);

#endif
