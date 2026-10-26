#define _POSIX_C_SOURCE 200809L
#include "timezone.h"
#include "config.h"
#include <ctype.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static bool installed_zone(const char *zone)
{
    /* Accept installed IANA names, never paths outside the timezone database
     * or arbitrary POSIX TZ expressions. TZif checks exclude metadata files. */
    size_t length = strlen(zone);
    if (!length || length > 127 || zone[0] == '/') return false;
    for (const unsigned char *p = (const unsigned char *)zone; *p; ++p)
        if (!isalnum(*p) && *p != '/' && *p != '_' && *p != '-' && *p != '+')
            return false;
    char filename[160];
    snprintf(filename, sizeof(filename), "/usr/share/zoneinfo/%s", zone);
    FILE *file = fopen(filename, "rb");
    if (!file) return false;
    char magic[4];
    bool valid = fread(magic, 1, sizeof(magic), file) == sizeof(magic) &&
                 !memcmp(magic, "TZif", sizeof(magic));
    fclose(file);
    return valid;
}

void pi_dos_timezone(const char *path)
{
    char selected[128] = "Etc/UTC";
    char zone[128];
    if (pi_dos_config_value(path, "pidos", "timezone", zone, sizeof(zone)) && *zone) {
        if (installed_zone(zone))
            memcpy(selected, zone, strlen(zone) + 1);
        else
            fputs("Pi-DOS: invalid [pidos] timezone; using Etc/UTC\n", stderr);
    }
    setenv("TZ", selected, 1);
    tzset();
    fprintf(stderr, "Pi-DOS: local timezone %s\n", selected);
}
