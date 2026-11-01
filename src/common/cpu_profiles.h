#ifndef PI_DOS_CPU_PROFILES_H
#define PI_DOS_CPU_PROFILES_H

#define PROFILE_COUNT 9
#define DEFAULT_CPU_PROFILE_ID "pentium-120"

typedef struct {
    const char *id;
    const char *label;
    const char *type;
    const char *core;
    const char *cycles;
    const char *description;
} CpuProfile;

extern const CpuProfile cpu_profiles[PROFILE_COUNT];
/* Pass NULL for core when identifying only the model and speed for display. */
int cpu_profile_find(const char *type, const char *core, const char *cycles);
int cpu_profile_default(void);
int cpu_profile_for_board(const char *model);
const char *cpu_profile_recommendation(int profile);

#endif
