#ifndef PIDOSCFG_CONFIG_H
#define PIDOSCFG_CONFIG_H

#include "cpu_profiles.h"
#define CFG_VALUE_SIZE 80
#define CFG_PATH_SIZE 128

extern char cfg_error[160];
extern char cfg_type[CFG_VALUE_SIZE];
extern char cfg_core[CFG_VALUE_SIZE];
extern char cfg_cycles[CFG_VALUE_SIZE];

int cfg_load(const char *path);
int cfg_profile(void);
int cfg_save(const char *path, int profile);

#endif
