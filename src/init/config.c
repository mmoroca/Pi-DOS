#define _POSIX_C_SOURCE 200809L
#include "config.h"
#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

static char *trim(char *text)
{
    while (isspace((unsigned char)*text)) ++text;
    char *end = text + strlen(text);
    while (end > text && isspace((unsigned char)end[-1])) *--end = 0;
    return text;
}

static bool section(const char *line, const char *name, bool *header)
{
    while (isspace((unsigned char)*line)) ++line;
    *header = false;
    if (*line++ != '[') return false;
    const char *end = strchr(line, ']');
    if (!end) return false;
    *header = true;
    while (line < end && isspace((unsigned char)*line)) ++line;
    while (end > line && isspace((unsigned char)end[-1])) --end;
    return (size_t)(end - line) == strlen(name) && !strncasecmp(line, name, end - line);
}

bool pi_dos_config_value(const char *path, const char *name, const char *key,
                         char *value, size_t size)
{
    if (!path || !size) return false;
    FILE *input = fopen(path, "r");
    if (!input) return false;
    char *line = NULL;
    size_t capacity = 0;
    bool active = false, found = false;
    while (getline(&line, &capacity, input) >= 0) {
        bool header;
        bool match = section(line, name, &header);
        if (header) { active = match; continue; }
        if (!active) continue;
        char *start = trim(line);
        if (*start == '#' || *start == ';') continue;
        char *equals = strchr(start, '=');
        if (!equals) continue;
        *equals++ = 0;
        if (strcasecmp(trim(start), key)) continue;
        equals[strcspn(equals, "#;")] = 0;
        char *setting = trim(equals);
        found = strlen(setting) < size;
        if (found) memcpy(value, setting, strlen(setting) + 1);
    }
    if (ferror(input)) found = false;
    free(line);
    fclose(input);
    return found;
}

bool pi_dos_runtime_config(const char *source, const char *destination)
{
    FILE *input = fopen(source, "r");
    if (!input) return false;
    FILE *output = fopen(destination, "w");
    if (!output) { fclose(input); return false; }
    char *line = NULL;
    size_t capacity = 0;
    bool custom = false;
    while (getline(&line, &capacity, input) >= 0) {
        bool header;
        bool match = section(line, "pidos", &header);
        if (header) custom = match;
        if (!custom) fputs(line, output);
    }
    bool valid = !ferror(input) && !ferror(output);
    free(line);
    fclose(input);
    if (fclose(output)) valid = false;
    return valid;
}

/* Translate the writable DOS startup file into the runtime's native CONFIG
 * section. Properties are applied before DOS initializes; DEVICE/INSTALL
 * commands run in order before AUTOEXEC. Never modify the user's source file. */
bool pi_dos_config_sys(const char *source, const char *destination)
{
    FILE *input = source ? fopen(source, "r") : NULL;
    if (source && !input && errno != ENOENT) return false;
    FILE *output = fopen(destination, "w");
    if (!output) { if (input) fclose(input); return false; }
    fputs("[config]\n", output);
    char *line = NULL;
    size_t capacity = 0;
    unsigned number = 0;
    bool in_section = false;
    while (input && getline(&line, &capacity, input) >= 0) {
        ++number;
        char *eof = strchr(line, 0x1a);
        if (eof) *eof = 0;
        char *start = trim(line);
        if (*start == '[') {
            /* DOS boot menus require selection and INCLUDE processing. Do not
             * accidentally load drivers from every alternative configuration. */
            in_section = true;
            dprintf(2, "Pi-DOS: CONFIG.SYS:%u: boot-menu sections are unsupported\n", number);
        }
        bool remark = !strncasecmp(start, "REM", 3) &&
                      (!start[3] || isspace((unsigned char)start[3]));
        if (!in_section && *start && *start != ';' && *start != '#' && !remark) {
            char *equals = strchr(start, '=');
            if (equals) *equals++ = 0;
            char *key = trim(start), *value = equals ? trim(equals) : NULL;
            for (char *at = key; *at; ++at) *at = (char)tolower((unsigned char)*at);
            bool property = !strcmp(key,"files") || !strcmp(key,"fcbs") ||
                !strcmp(key,"dos") || !strcmp(key,"lastdrive") ||
                !strcmp(key,"break") || !strcmp(key,"numlock") || !strcmp(key,"country");
            bool command = !strcmp(key,"device") || !strcmp(key,"devicehigh") ||
                !strcmp(key,"install") || !strcmp(key,"installhigh") || !strncmp(key,"set ",4);
            bool memory_manager = false;
            if (value && (!strcmp(key,"device") || !strcmp(key,"devicehigh"))) {
                /* XMS/EMS are provided by the internal DOS kernel. Loading a
                 * second memory manager can disrupt protected-mode programs. */
                char path[512];
                snprintf(path, sizeof(path), "%s", value);
                char *token = path + (path[0] == '"');
                char *end = path[0] == '"' ? strchr(token, '"') : strpbrk(token, " \t");
                if (end) *end = 0;
                char *name = token;
                for (char *at = token; *at; ++at)
                    if (*at == '\\' || *at == '/' || *at == ':') name = at + 1;
                memory_manager = !strcasecmp(name,"himem.sys") || !strcasecmp(name,"emm386.exe");
            }
            if (value && *value && (property || command) && !memory_manager)
                fprintf(output, "%s=%s\n", key, value);
            else
                dprintf(2, "Pi-DOS: CONFIG.SYS:%u: skipping %s%s\n", number, key,
                        memory_manager ? " (internal memory manager active)" : " (unsupported or empty directive)");
        }
        if (eof) break;
    }
    bool valid = (!input || !ferror(input)) && !ferror(output);
    free(line);
    if (input) fclose(input);
    if (fclose(output)) valid = false;
    return valid;
}

/* Appliance-only options are passed outside the DOSBox configuration. */
#define PIDOS_INPUT_OPTIONS_VALIDATE
#include "input_options.h"
void pi_dos_input_prepare(const char *source)
{
    const char *keys[] = {"controller_layout", "controller_direction", "controller_swap_buttons"};
    const char *variables[] = {"PI_DOS_CONTROLLER_LAYOUT", "PI_DOS_CONTROLLER_DIRECTION", "PI_DOS_CONTROLLER_SWAP_BUTTONS"};
    const char *defaults[] = {"auto", "both", "false"};
    char value[80];
    for (size_t i=0; i<3; ++i) {
        bool valid = pi_dos_config_value(source, "pidos", keys[i], value, sizeof(value));
        if (i==0) valid = valid && (!strcasecmp(value,"auto") || !strcasecmp(value,"raw"));
        if (i==1) valid = valid && (!strcasecmp(value,"both") || !strcasecmp(value,"stick") || !strcasecmp(value,"dpad"));
        if (i==2) valid = valid && (!strcasecmp(value,"true") || !strcasecmp(value,"false"));
        if (!valid) snprintf(value,sizeof(value),"%s",defaults[i]);
        for (char *at=value; *at; ++at) *at=(char)tolower((unsigned char)*at);
        setenv(variables[i],value,1);
    }
    const char *shortcuts[] = {"shortcut_reboot", "shortcut_poweroff", "shortcut_pause",
                              "shortcut_screenshot", "shortcut_next_cd", "shortcut_next_floppy"};
    const char *env[] = {"PI_DOS_SHORTCUT_REBOOT", "PI_DOS_SHORTCUT_POWEROFF", "PI_DOS_SHORTCUT_PAUSE",
                        "PI_DOS_SHORTCUT_SCREENSHOT", "PI_DOS_SHORTCUT_NEXT_CD", "PI_DOS_SHORTCUT_NEXT_FLOPPY"};
    const int shortcut_defaults[] = {1, 0, 0, 12, 5, 6};
    int selected[6] = {0};
    for (size_t i=0; i<6; ++i) {
        int code = shortcut_defaults[i];
        if (pi_dos_config_value(source,"pidos",shortcuts[i],value,sizeof(value))) {
            int parsed = pidos_shortcut_key(value);
            if (parsed>=0) code=parsed;
            else dprintf(2,"Pi-DOS: invalid %s; using default\n",shortcuts[i]);
        }
        for (size_t j=0; j<i; ++j) if (code && code==selected[j]) {
            dprintf(2,"Pi-DOS: duplicate %s disabled\n",shortcuts[i]); code=0;
        }
        selected[i]=code;
        snprintf(value,sizeof(value),"%d",code);
        setenv(env[i],value,1);
    }
}
