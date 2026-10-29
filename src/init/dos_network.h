#ifndef PI_DOS_DOS_NETWORK_H
#define PI_DOS_DOS_NETWORK_H
#include <stdbool.h>
#include <stdio.h>
/* Create generated Z:\NETWORK launchers before the read-only root remount. */
bool pi_dos_dos_network(const char *config, const char *drivez, const char *data,
                        bool writable, FILE *autoexec);
#endif
