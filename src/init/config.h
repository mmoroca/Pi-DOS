#ifndef PI_DOS_CONFIG_H
#define PI_DOS_CONFIG_H

#include <stdbool.h>
#include <stddef.h>

bool pi_dos_config_value(const char *path, const char *section, const char *key,
                         char *value, size_t size);
bool pi_dos_runtime_config(const char *source, const char *destination);
bool pi_dos_config_sys(const char *source, const char *destination);
void pi_dos_input_prepare(const char *source);

#endif
