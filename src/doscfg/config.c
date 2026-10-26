/* Pi-DOS configuration utility. C89, also compiled for 8086 real-mode DOS. */
#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#ifdef __WATCOMC__
#include <dos.h>
#endif
#include "config.h"

#define CFG_CAPACITY 24576

char cfg_error[160];
char cfg_type[CFG_VALUE_SIZE];
char cfg_core[CFG_VALUE_SIZE];
char cfg_cycles[CFG_VALUE_SIZE];
static char contents[CFG_CAPACITY + 1];
static unsigned int content_size;
static int file_exists;

static int same(const char *a, const char *b)
{
    while (*a && *b) {
        if (tolower((unsigned char)*a++) != tolower((unsigned char)*b++)) return 0;
    }
    return *a == *b;
}

/* Parse spans without modifying the original bytes or limiting line length. */
static void value_span(unsigned int start, unsigned int end, char *value)
{
    unsigned int n;
    while (start < end && isspace((unsigned char)contents[start])) ++start;
    while (end > start && isspace((unsigned char)contents[end-1])) --end;
    n = end - start;
    if (n >= CFG_VALUE_SIZE) n = CFG_VALUE_SIZE - 1;
    memcpy(value, contents + start, n);
    value[n] = 0;
}

/* 0 = other, 1 = section, 2/3/4 = CPU type/core/cycles assignment. */
static int parse_line(unsigned int start, unsigned int end, int *cpu,
                      unsigned int *equals)
{
    unsigned int i;
    char key[CFG_VALUE_SIZE];
    while (start < end && isspace((unsigned char)contents[start])) ++start;
    if (end - start >= 14 && !memcmp(contents + start, "#DOSBOX-X-ADV:", 14)) start += 14;
    else if (end - start >= 10 && !memcmp(contents + start, "#DOSBOX-X:", 10)) start += 10;
    while (start < end && isspace((unsigned char)contents[start])) ++start;
    if (start == end || contents[start] == '#' || contents[start] == ';' ||
        contents[start] == '%') return 0;
    if (contents[start] == '[') {
        for (i = start + 1; i < end && contents[i] != ']'; ++i) {}
        if (i == end) return 0;
        value_span(start + 1, i, key);
        *cpu = same(key, "cpu");
        return 1;
    }
    if (!*cpu) return 0;
    for (i = start; i < end && contents[i] != '='; ++i) {}
    if (i == end) return 0;
    value_span(start, i, key);
    *equals = i;
    if (same(key, "cputype")) return 2;
    if (same(key, "core")) return 3;
    if (same(key, "cycles")) return 4;
    return 0;
}

int cfg_load(const char *path)
{
    FILE *input;
    unsigned int start, end, equals;
    int cpu = 0, kind, extra, failed;
    const CpuProfile *defaults = &cpu_profiles[cpu_profile_default()];
    cfg_error[0] = 0;
    content_size = 0;
    file_exists = 0;
    strcpy(cfg_type, defaults->type);
    strcpy(cfg_core, defaults->core);
    strcpy(cfg_cycles, defaults->cycles);
    input = fopen(path, "rb");
    if (!input) {
        if (errno == ENOENT) return 1;
        strcpy(cfg_error, "Cannot read the configuration file.");
        return 0;
    }
    file_exists = 1;
    content_size = (unsigned int)fread(contents, 1, CFG_CAPACITY, input);
    extra = fgetc(input);
    failed = ferror(input);
    if (fclose(input)) failed = 1;
    if (failed || extra != EOF || memchr(contents, 0, content_size)) {
        strcpy(cfg_error, "Cannot edit: file is unreadable, binary, or larger than 24 KiB.");
        return 0;
    }
    contents[content_size] = 0;
    for (start = 0; start < content_size; start = end) {
        for (end = start; end < content_size && contents[end] != '\n'; ++end) {}
        kind = parse_line(start, end, &cpu, &equals);
        if (kind >= 2) {
            value_span(equals + 1, end,
                       kind == 2 ? cfg_type : kind == 3 ? cfg_core : cfg_cycles);
        }
        if (end < content_size) ++end;
    }
    return 1;
}

int cfg_profile(void)
{
    return cpu_profile_find(cfg_type, cfg_core, cfg_cycles);
}

static int write_span(FILE *out, unsigned int start, unsigned int end)
{
    return fwrite(contents + start, 1, end - start, out) == end - start;
}

static int write_config(FILE *out, const CpuProfile *profile)
{
    unsigned int start, end, next, equals;
    int cpu = 0, kind, seen[3] = {0, 0, 0}, missing, i;
    const char *keys[3] = {"cputype", "core", "cycles"};
    const char *values[3];
    values[0] = profile->type;
    values[1] = profile->core;
    values[2] = profile->cycles;
    for (start = 0; start < content_size; start = next) {
        for (end = start; end < content_size && contents[end] != '\n'; ++end) {}
        next = end < content_size ? end + 1 : end;
        kind = parse_line(start, end, &cpu, &equals);
        if (kind >= 2) {
            seen[kind-2] = 1;
            /* Keep indentation, key spelling and all unrelated lines verbatim. */
            if (!write_span(out, start, equals + 1) ||
                fprintf(out, "%s\r\n", values[kind-2]) < 0) return 0;
        } else if (!write_span(out, start, next)) return 0;
    }
    missing = !seen[0] || !seen[1] || !seen[2];
    if (missing) {
        if (content_size && contents[content_size-1] != '\n' &&
            fputs("\r\n", out) == EOF) return 0;
        if (fputs("\r\n[cpu]\r\n", out) == EOF) return 0;
        for (i = 0; i < 3; ++i) {
            if (!seen[i] && fprintf(out, "%s=%s\r\n", keys[i], values[i]) < 0)
                return 0;
        }
    }
    return !ferror(out) && ftell(out) <= CFG_CAPACITY;
}

int cfg_save(const char *path, int profile)
{
    char temporary[CFG_PATH_SIZE], backup[CFG_PATH_SIZE];
    char *dot, *slash, *backslash, *base;
    FILE *out;
    int ok;
#ifdef __WATCOMC__
    unsigned int attributes;
#endif
    if (profile < 0 || profile >= PROFILE_COUNT) {
        strcpy(cfg_error, "Select a CPU profile first.");
        return 0;
    }
    if (strlen(path) >= CFG_PATH_SIZE - 5) {
        strcpy(cfg_error, "Configuration path is too long.");
        return 0;
    }
    /* Reload before saving so unrelated edits since startup are retained. */
    if (!cfg_load(path)) return 0;
#ifdef __WATCOMC__
    if (file_exists && (_dos_getfileattr(path, &attributes) || (attributes & _A_RDONLY))) {
        strcpy(cfg_error, "Configuration is read-only or inaccessible. No changes saved.");
        return 0;
    }
#endif
    strcpy(temporary, path);
    slash = strrchr(temporary, '/');
    backslash = strrchr(temporary, '\\');
    base = temporary;
    if (slash) base = slash + 1;
    if (backslash && backslash >= base) base = backslash + 1;
    dot = strrchr(base, '.');
    if (dot) *dot = 0;
    strcpy(backup, temporary);
    strcat(temporary, ".TMP");
    strcat(backup, ".BAK");
    if (same(path, temporary) || same(path, backup)) {
        strcpy(cfg_error, "Use a configuration filename other than .TMP or .BAK.");
        return 0;
    }
    /* Refuse to overwrite a recovery file from an interrupted save. */
    out = fopen(temporary, "rb");
    if (out) {
        fclose(out);
        strcpy(cfg_error, "A .TMP recovery file exists. Check it before saving again.");
        return 0;
    }
    out = fopen(temporary, "wb");
    if (!out) {
        strcpy(cfg_error, "Cannot write configuration. Check free space and read-only storage.");
        return 0;
    }
    ok = write_config(out, &cpu_profiles[profile]);
    if (fclose(out)) ok = 0;
    if (!ok) {
        remove(temporary);
        strcpy(cfg_error, "Write failed. The original configuration has not been changed.");
        return 0;
    }
    if (file_exists) {
        if (remove(backup) && errno != ENOENT) {
            remove(temporary);
            strcpy(cfg_error, "Cannot replace the previous .BAK backup. Configuration unchanged.");
            return 0;
        }
        if (rename(path, backup)) {
            remove(temporary);
            strcpy(cfg_error, "Cannot back up configuration. The original file is unchanged.");
            return 0;
        }
    }
    if (rename(temporary, path)) {
        if (file_exists && rename(backup, path)) {
            strcpy(cfg_error, "Save failed. Restore the .BAK file; .TMP contains the new settings.");
        } else {
            remove(temporary);
            strcpy(cfg_error, "Save failed. The original configuration has been restored.");
        }
        return 0;
    }
    cfg_load(path);
    return 1;
}
