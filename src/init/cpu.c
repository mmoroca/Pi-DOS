#define _POSIX_C_SOURCE 200809L
#include "cpu.h"
#include "config.h"
#include "cpu_profiles.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>
#include <unistd.h>

int pi_dos_cpu_profile(void)
{
    char model[256] = "";
    FILE *input = fopen("/sys/firmware/devicetree/base/model", "r");
    if (input) {
        if (!fgets(model, sizeof(model), input)) model[0] = 0;
        fclose(input);
    }
    return cpu_profile_for_board(model);
}

static bool name_is(const char *start, const char *end, const char *name)
{
    while (start < end && isspace((unsigned char)*start)) ++start;
    while (end > start && isspace((unsigned char)end[-1])) --end;
    return (size_t)(end - start) == strlen(name) && !strncasecmp(start, name, end - start);
}

/* Only fresh templates opt in. Resolve before DOS starts and persist once, so
 * updates and moving an existing card to another board keep the user's CPU. */
bool pi_dos_cpu_defaults(const char *path, int profile, bool writable)
{
    char state[32], type[80], core[80], cycles[80];
    if (!writable || !path || profile < 0 || profile >= PROFILE_COUNT ||
        !pi_dos_config_value(path, "pidos", "cpu_default", state, sizeof(state)) ||
        strcasecmp(state, "pending") ||
        !pi_dos_config_value(path, "pidos", "first_boot", state, sizeof(state)) ||
        strcasecmp(state, "pending")) return true;
    /* Honor a CPU customized in the template before its first boot too. */
    const CpuProfile *baseline = &cpu_profiles[cpu_profile_default()];
    bool replace = pi_dos_config_value(path, "cpu", "cputype", type, sizeof(type)) &&
                   pi_dos_config_value(path, "cpu", "core", core, sizeof(core)) &&
                   pi_dos_config_value(path, "cpu", "cycles", cycles, sizeof(cycles)) &&
                   cpu_profile_find(type, core, cycles) == cpu_profile_default();
    const CpuProfile *selected = replace ? &cpu_profiles[profile] : baseline;
    FILE *input = fopen(path, "r");
    if (!input) return false;
    size_t length = strlen(path) + sizeof(".XXXXXX");
    char *temporary = malloc(length);
    if (!temporary) { fclose(input); return false; }
    snprintf(temporary, length, "%s.XXXXXX", path);
    int fd = mkstemp(temporary);
    FILE *output = fd >= 0 ? fdopen(fd, "w") : NULL;
    if (!output) {
        if (fd >= 0) { close(fd); unlink(temporary); }
        free(temporary); fclose(input); return false;
    }
    char *line = NULL;
    size_t capacity = 0;
    bool cpu = false, pidos = false;
    while (getline(&line, &capacity, input) >= 0) {
        char *start = line;
        while (isspace((unsigned char)*start)) ++start;
        const char *value = NULL;
        char *equals = strchr(start, '=');
        if (*start == '[') {
            char *end = strchr(start, ']');
            cpu = end && name_is(start + 1, end, "cpu");
            pidos = end && name_is(start + 1, end, "pidos");
        } else if (*start != '#' && *start != ';' && equals) {
            if (pidos && name_is(start, equals, "cpu_default")) value = "done";
            if (cpu && replace) {
                if (name_is(start, equals, "cputype")) value = selected->type;
                if (name_is(start, equals, "core")) value = selected->core;
                if (name_is(start, equals, "cycles")) value = selected->cycles;
            }
        }
        if (value) {
            char *comment = strpbrk(equals + 1, "#;");
            fprintf(output, "%.*s%s%s%s", (int)(equals + 1 - line), line, value,
                    comment ? " " : "", comment ? comment :
                    (strchr(line, '\r') ? "\r\n" : "\n"));
        } else fputs(line, output);
    }
    bool valid = !ferror(input) && !ferror(output);
    free(line);
    fclose(input);
    if (fflush(output) || fchmod(fd, 0644) || fsync(fd)) valid = false;
    if (fclose(output)) valid = false;
    if (valid && rename(temporary, path)) valid = false;
    if (!valid) unlink(temporary);
    free(temporary);
    return valid;
}
