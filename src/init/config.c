#define _POSIX_C_SOURCE 200809L
#include "config.h"
#include <ctype.h>
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
