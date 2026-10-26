/* Shared by the 8086 DOS setup utility and the native ARM64 boot summary. */
#include <ctype.h>
#include <string.h>
#include "cpu_profiles.h"

const CpuProfile cpu_profiles[PROFILE_COUNT] = {
    {"auto", "Automatic", "auto", "dynamic_rec", "auto",
     "Automatically choose execution speed for the running program."},
    {"286-12", "286 12 MHz", "286", "normal", "fixed 1510",
     "A slower AT-class configuration for earlier DOS software."},
    {"386-33", "386DX 33 MHz", "386", "dynamic_rec", "fixed 6075",
     "A representative 386 configuration for DOS applications and games."},
    {"486-33", "486DX 33 MHz", "486", "dynamic_rec", "fixed 12010",
     "A lower-speed 486 configuration."},
    {"486-66", "486DX2 66 MHz", "486", "dynamic_rec", "fixed 23880",
     "A classic middle-ground 486 configuration."},
    {"486-100", "486DX4 100 MHz", "486", "dynamic_rec", "fixed 33445",
     "A faster 486 configuration."},
    {"pentium-100", "Pentium 100 MHz", "pentium", "dynamic_rec", "fixed 53430",
     "A representative Pentium configuration for later DOS software."},
    {"pentium-133", "Pentium 133 MHz", "pentium", "dynamic_rec", "fixed 71240",
     "A faster Pentium configuration for later DOS software."},
    {"mmx-200", "Pentium MMX 200 MHz", "pentium_mmx", "dynamic_rec", "fixed 114657",
     "A fast late-DOS-era configuration with MMX instructions."}
};

static int same(const char *a, const char *b)
{
    while (*a && *b) {
        if (tolower((unsigned char)*a++) != tolower((unsigned char)*b++)) return 0;
    }
    return *a == *b;
}

static const char *cycle_value(const char *text)
{
    /* Both cycles=3000 and cycles=fixed 3000 mean a fixed budget. */
    if (strlen(text) >= 6 && tolower((unsigned char)text[0]) == 'f' &&
        tolower((unsigned char)text[1]) == 'i' &&
        tolower((unsigned char)text[2]) == 'x' &&
        tolower((unsigned char)text[3]) == 'e' &&
        tolower((unsigned char)text[4]) == 'd' && isspace((unsigned char)text[5])) {
        text += 6;
        while (isspace((unsigned char)*text)) ++text;
    }
    return text;
}

int cpu_profile_find(const char *type, const char *core, const char *cycles)
{
    int i;
    for (i = 0; i < PROFILE_COUNT; ++i) {
        const CpuProfile *p = &cpu_profiles[i];
        if (same(type, p->type) && (!core || same(core, p->core)) &&
            same(cycle_value(cycles), cycle_value(p->cycles))) return i;
    }
    return -1;
}

int cpu_profile_default(void)
{
    int i;
    for (i = 0; i < PROFILE_COUNT; ++i)
        if (!strcmp(cpu_profiles[i].id, DEFAULT_CPU_PROFILE_ID)) return i;
    return 0;
}
