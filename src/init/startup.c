/* Boot summary rendered by the DOS shell in its native 80x25 text mode. */
#define _GNU_SOURCE
#include "startup.h"
#include <ctype.h>
#include <glob.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/statvfs.h>
#include <sys/sysinfo.h>
#include <unistd.h>

#ifndef PI_DOS_VERSION
#define PI_DOS_VERSION "Unknown"
#endif

/* 75 columns, leaving room for the DOS cursor without wrapping. Use double-line
 * box characters shared by CP437 and CP850 (Spanish); mixed single/double
 * junctions become accented letters in CP850. These characters also avoid the
 * shell's ASCII pipe/redirection operators. */
enum { CELL = 36, WIDTH = CELL * 2 + 1 };

static void safe_text(char *text)
{
    /* Hardware/config strings become ECHO arguments, never DOS commands. */
    for (unsigned char *p = (unsigned char *)text; *p; ++p)
        if (*p < 32 || *p > 126 || strchr("&|<>^%\"", *p)) *p = '?';
}

static bool read_text(const char *path, char *text, size_t size)
{
    FILE *file = fopen(path, "r");
    if (!file) return false;
    bool ok = fgets(text, (int)size, file) != NULL;
    fclose(file);
    if (ok) {
        text[strcspn(text, "\r\n")] = 0;
        safe_text(text);
    }
    return ok && text[0];
}

static char *trim(char *text)
{
    while (isspace((unsigned char)*text)) ++text;
    size_t length = strlen(text);
    while (length && isspace((unsigned char)text[length - 1])) text[--length] = 0;
    return text;
}

static void setting(FILE *file, const char *section, const char *key,
                    char *value, size_t size)
{
    if (!file) return;
    rewind(file);
    bool active = false;
    char line[4096];
    while (fgets(line, sizeof(line), file)) {
        char *text = trim(line);
        if (*text == '#' || *text == ';') continue;
        if (*text == '[') {
            char *end = strchr(++text, ']');
            if (end) *end = 0;
            active = end && !strcasecmp(trim(text), section);
            continue;
        }
        char *equals = strchr(text, '=');
        if (!active || !equals) continue;
        *equals++ = 0;
        if (!strcasecmp(trim(text), key)) {
            snprintf(value, size, "%s", trim(equals));
            safe_text(value);
        }
    }
    if (!*value) snprintf(value, size, "Unknown");
}

static void size_text(char *text, size_t size, uint64_t bytes)
{
    static const char *units[] = {"B", "KiB", "MiB", "GiB", "TiB"};
    double amount = (double)bytes;
    unsigned int unit = 0;
    while (amount >= 1024 && unit < 4) { amount /= 1024; ++unit; }
    snprintf(text, size, "%.1f %s", amount, units[unit]);
}

static void border(FILE *out, unsigned char horizontal, unsigned char left,
                   unsigned char middle, unsigned char right)
{
    fprintf(out, "ECHO %c", left);
    for (int i = 0; i < WIDTH; ++i)
        fputc(i == CELL && middle ? middle : horizontal, out);
    fprintf(out, "%c\n", right);
}

static void full_row(FILE *out, const char *text)
{
    fprintf(out, "ECHO \xba %-*.*s \xba\n", WIDTH - 2, WIDTH - 2, text);
}

static void pair_row(FILE *out, const char *label, const char *value,
                     const char *other_label, const char *other_value)
{
    fprintf(out, "ECHO \xba %-10.10s : %-21.21s \xba %-10.10s : %-21.21s \xba\n",
            label, value, other_label, other_value);
}

void pi_dos_startup(FILE *out, const char *config, const char *user_config, const char *device,
                    bool mounted, bool writable, bool drive_c)
{
    char board[256] = "Unknown", cpu[128] = "Unknown", cores[32] = "Unknown";
    char ram[64] = "Unknown", disk[128] = "Unknown", summary[256];
    read_text("/sys/firmware/devicetree/base/model", board, sizeof(board));
    glob_t paths = {0};
    if (!glob("/sys/firmware/devicetree/base/cpus/cpu@*/compatible", 0, NULL, &paths)) {
        for (size_t i = 0; i < paths.gl_pathc; ++i) {
            char compatible[128];
            if (read_text(paths.gl_pathv[i], compatible, sizeof(compatible)) &&
                !strncmp(compatible, "arm,cortex-", 11)) {
                for (char *p = compatible + 11; *p; ++p) *p = toupper((unsigned char)*p);
                snprintf(cpu, sizeof(cpu), "ARM Cortex-%.100s", compatible + 11);
                break;
            }
        }
    }
    globfree(&paths);
    long count = sysconf(_SC_NPROCESSORS_ONLN);
    if (count > 0) snprintf(cores, sizeof(cores), "%ld", count);
    struct sysinfo info;
    if (!sysinfo(&info)) {
        char amount[32];
        size_text(amount, sizeof(amount), (uint64_t)info.totalram * info.mem_unit);
        snprintf(ram, sizeof(ram), "%s usable", amount);
    }

    /* Report the actual selected block device, including USB/NVMe boot data. */
    snprintf(disk, sizeof(disk), "%s", device);
    safe_text(disk);
    FILE *file = fopen(config, "r");
    char graphics[128] = "Unknown", memory[128] = "Unknown";
    char sound[128] = "Unknown", keyboard[128] = "Unknown";
    setting(file, "dosbox", "machine", graphics, sizeof(graphics));
    setting(file, "dosbox", "memsize", memory, sizeof(memory));
    setting(file, "sblaster", "sbtype", sound, sizeof(sound));
    setting(file, "dos", "keyboardlayout", keyboard, sizeof(keyboard));
    if (file) fclose(file);
    file = user_config ? fopen(user_config, "r") : NULL;
    setting(file, "dosbox", "machine", graphics, sizeof(graphics));
    setting(file, "dosbox", "memsize", memory, sizeof(memory));
    setting(file, "sblaster", "sbtype", sound, sizeof(sound));
    setting(file, "dos", "keyboardlayout", keyboard, sizeof(keyboard));
    if (file) fclose(file);
    if (!strcasecmp(graphics, "svga_s3")) snprintf(graphics, sizeof(graphics), "S3 SVGA");
    if (!strcasecmp(sound, "sb16")) snprintf(sound, sizeof(sound), "Sound Blaster 16");
    if (!strcmp(memory, "Unknown")) snprintf(summary, sizeof(summary), "Unknown");
    else snprintf(summary, sizeof(summary), "%s MiB", memory);
    struct statvfs space;
    bool have_space = mounted && !statvfs("/dosdata", &space);
    if (have_space && (space.f_flag & ST_RDONLY)) writable = false;

    border(out, 0xcd, 0xc9, 0, 0xbb);
    full_row(out, "                         System Configuration");
    border(out, 0xcd, 0xcc, 0, 0xb9);
    char board_row[300];
    snprintf(board_row, sizeof(board_row), "Board      : %s", board);
    full_row(out, board_row);
    border(out, 0xcd, 0xcc, 0xcb, 0xb9);
    pair_row(out, "Processor", cpu, "System", "Pi-DOS " PI_DOS_VERSION);
    pair_row(out, "CPU cores", cores, "DOS video", graphics);
    pair_row(out, "Host RAM", ram, "DOS memory", summary);
    pair_row(out, "Storage", disk, "DOS audio", sound);
    pair_row(out, "Access", !mounted ? "Unavailable" : writable ? "Read/write" : "Read-only",
             "Keyboard", keyboard);
    border(out, 0xcd, 0xcc, 0xca, 0xb9);
    if (have_space) {
        char total[32], free_space[32];
        size_text(total, sizeof(total), (uint64_t)space.f_blocks * space.f_frsize);
        size_text(free_space, sizeof(free_space), (uint64_t)space.f_bavail * space.f_frsize);
        snprintf(summary, sizeof(summary), "PI-DOS     : %s total / %s free", total, free_space);
    } else {
        snprintf(summary, sizeof(summary), "PI-DOS     : %s", mounted ? "Unknown" : "Unavailable");
    }
    full_row(out, summary);
    full_row(out, !drive_c ? "Drives     : Z: built-in DOS utilities (recovery)" :
             "Drives     : C: on PI-DOS; Z: built-in DOS utilities");
    border(out, 0xcd, 0xc8, 0, 0xbc);
    fputs("ECHO.\nECHO Welcome to Pi-DOS\nECHO.\n", out);
}
