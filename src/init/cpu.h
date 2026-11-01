#ifndef PI_DOS_CPU_H
#define PI_DOS_CPU_H

#include <stdbool.h>

int pi_dos_cpu_profile(void);
bool pi_dos_cpu_defaults(const char *path, int profile, bool writable);

#endif
