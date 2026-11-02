#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include "audio_ini.h"

static int equal(const char *a, const char *b)
{
    while (*a && *b && tolower((unsigned char)*a) == tolower((unsigned char)*b)) { ++a; ++b; }
    return !*a && !*b;
}

int pa_sound_vxd(const char *input, const char *output, const char *device)
{
    FILE *in = fopen(input, "rb"), *out;
    static char line[1024], token[1024];
    char *p, *end, *base;
    int enhanced = 0, seen = 0, ok = 1;
    if (!in) return 0;
    out = fopen(output, "wb");
    if (!out) { fclose(in); return 0; }
    while (fgets(line, sizeof(line), in)) {
        /* Fail rather than split and alter an unusually long INI line. */
        if (!strchr(line, '\n') && !feof(in)) { ok = 0; break; }
        strcpy(token, line);
        p = token;
        while (isspace((unsigned char)*p)) ++p;
        if (*p == '[') {
            if (enhanced && device && *device && !seen) {
                if (fprintf(out, "device=%s\r\n", device) < 0) ok = 0;
                seen = 1;
            }
            end = strchr(p, ']');
            if (end) *end = 0;
            enhanced = end && equal(p + 1, "386Enh");
        } else if (enhanced) {
            end = strchr(p, '=');
            if (end) {
                *end++ = 0;
                base = p + strlen(p);
                while (base > p && isspace((unsigned char)base[-1])) *--base = 0;
                if (equal(p, "device")) {
                    while (isspace((unsigned char)*end)) ++end;
                    p = end;
                    while (*p && *p != ';' && !isspace((unsigned char)*p)) ++p;
                    *p = 0;
                    if (*end == '"') { ++end; p = strchr(end, '"'); if (p) *p = 0; }
                    base = strrchr(end, '\\'); base = base ? base + 1 : end;
                    if (equal(base, "vsbpd.386") || equal(base, "vsbd.386") ||
                            equal(base, "vadlibd.386")) continue;
                }
            }
        }
        if (fputs(line, out) == EOF) ok = 0;
    }
    if (ferror(in)) ok = 0;
    if (device && *device && !seen) {
        if (!enhanced && fputs("\r\n[386Enh]\r\n", out) == EOF) ok = 0;
        if (fprintf(out, "\r\ndevice=%s\r\n", device) < 0) ok = 0;
    }
    fclose(in);
    if (fclose(out)) ok = 0;
    if (!ok) remove(output);
    return ok;
}
