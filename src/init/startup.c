/* Boot summary rendered by the DOS shell in its native 80x25 text mode. */
#define _GNU_SOURCE
#include "startup.h"
#include "cpu_profiles.h"
#include "video_profiles.h"
#include "midi.h"
#include <ctype.h>
#include <glob.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/statvfs.h>
#include <sys/sysinfo.h>
#include <time.h>
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
    char left[35] = "", other[35] = "";
    if (*label)
        snprintf(left, sizeof(left), "%-10.10s : %-21.21s", label, value);
    if (*other_label)
        snprintf(other, sizeof(other), "%-10.10s : %-21.21s", other_label, other_value);
    fprintf(out, "ECHO \xba %-34.34s \xba %-34.34s \xba\n", left, other);
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
    char country[128] = "Unknown", voodoo[32] = "software", zone[128];
    snprintf(zone, sizeof(zone), "%s", getenv("TZ") ? getenv("TZ") : "Etc/UTC");
    safe_text(zone);
    char gus[16] = "false", tandy[16] = "off", opl[16] = "auto";
    char dos_cpu[128], cycles[128], cpu_model[128];
    const CpuProfile *defaults = &cpu_profiles[cpu_profile_default()];
    snprintf(dos_cpu, sizeof(dos_cpu), "%s", defaults->type);
    snprintf(cycles, sizeof(cycles), "%s", defaults->cycles);
    setting(file, "dosbox", "machine", graphics, sizeof(graphics));
    setting(file, "dosbox", "memsize", memory, sizeof(memory));
    setting(file, "sblaster", "sbtype", sound, sizeof(sound));
    setting(file, "sblaster", "oplmode", opl, sizeof(opl));
    setting(file, "gus", "gus", gus, sizeof(gus));
    setting(file, "speaker", "tandy", tandy, sizeof(tandy));
    setting(file, "dos", "keyboardlayout", keyboard, sizeof(keyboard));
    setting(file, "config", "country", country, sizeof(country));
    setting(file, "voodoo", "voodoo_card", voodoo, sizeof(voodoo));
    setting(file, "cpu", "cputype", dos_cpu, sizeof(dos_cpu));
    setting(file, "cpu", "cycles", cycles, sizeof(cycles));
    if (file) fclose(file);
    file = user_config ? fopen(user_config, "r") : NULL;
    setting(file, "dosbox", "machine", graphics, sizeof(graphics));
    setting(file, "dosbox", "memsize", memory, sizeof(memory));
    setting(file, "sblaster", "sbtype", sound, sizeof(sound));
    setting(file, "sblaster", "oplmode", opl, sizeof(opl));
    setting(file, "gus", "gus", gus, sizeof(gus));
    setting(file, "speaker", "tandy", tandy, sizeof(tandy));
    setting(file, "dos", "keyboardlayout", keyboard, sizeof(keyboard));
    setting(file, "config", "country", country, sizeof(country));
    setting(file, "voodoo", "voodoo_card", voodoo, sizeof(voodoo));
    setting(file, "cpu", "cputype", dos_cpu, sizeof(dos_cpu));
    setting(file, "cpu", "cycles", cycles, sizeof(cycles));
    if (file) fclose(file);
    int profile = cpu_profile_find(dos_cpu, NULL, cycles);
    if (!strcasecmp(dos_cpu, "auto"))
        snprintf(cpu_model, sizeof(cpu_model), "Auto (486/Pentium)");
    else if (profile >= 1)
        snprintf(cpu_model, sizeof(cpu_model), "%s", cpu_profiles[profile].label);
    else if (!strcasecmp(dos_cpu, "pentium_mmx"))
        snprintf(cpu_model, sizeof(cpu_model), "Pentium MMX");
    else if (!strcasecmp(dos_cpu, "pentium"))
        snprintf(cpu_model, sizeof(cpu_model), "Pentium");
    else snprintf(cpu_model, sizeof(cpu_model), "%s", dos_cpu);
    bool gus_on = !strcasecmp(gus, "true");
    bool tandy_on = !strcasecmp(tandy, "on") ||
                    (!strcasecmp(tandy, "auto") && !strcasecmp(graphics, "tandy"));
    bool sb_on = strcasecmp(sound, "none") && strcasecmp(sound, "Unknown");
    bool adlib_on = !sb_on && strcasecmp(opl, "none") && strcasecmp(opl, "auto");
    if ((int)gus_on + (int)tandy_on + (int)sb_on + (int)adlib_on > 1)
        snprintf(sound, sizeof(sound), "Multiple devices");
    else if (gus_on) snprintf(sound, sizeof(sound), "GUS Classic");
    else if (tandy_on) snprintf(sound, sizeof(sound), "Tandy sound");
    else if (adlib_on) snprintf(sound, sizeof(sound), "AdLib");
    else if (!strcasecmp(sound, "sb16")) snprintf(sound, sizeof(sound), "Sound Blaster 16");
    else if (!strcasecmp(sound, "sbpro2")) snprintf(sound, sizeof(sound), "Sound Blaster Pro 2");
    else if (!strcasecmp(sound, "sb2.0")) snprintf(sound, sizeof(sound), "Sound Blaster 2.0");
    else if (!strcasecmp(sound, "gb")) snprintf(sound, sizeof(sound), "Game Blaster / CMS");
    else if (!strcasecmp(sound, "none")) snprintf(sound, sizeof(sound), "None");
    int video = video_profile_find(graphics);
    if (video >= 0) snprintf(graphics, sizeof(graphics), "%s", video_profiles[video].label);
    const char *voodoo_status = !strcasecmp(voodoo, "software") || !strcasecmp(voodoo, "auto")
                              ? "Enabled" : "Disabled";
    char midi[80]; pi_dos_midi_label(user_config, midi, sizeof(midi)); safe_text(midi);
    if (!strcmp(memory, "Unknown")) snprintf(summary, sizeof(summary), "Unknown");
    else snprintf(summary, sizeof(summary), "%s MiB", memory);
    struct statvfs space;
    bool have_space = mounted && !statvfs("/dosdata", &space);
    if (have_space && (space.f_flag & ST_RDONLY)) writable = false;
    char date[32] = "Unknown", clock[32] = "Unknown";
    time_t now = time(NULL);
    struct tm local;
    if (now != (time_t)-1 && localtime_r(&now, &local)) {
        if (!strftime(date, sizeof(date), "%Y-%m-%d", &local))
            snprintf(date, sizeof(date), "Unknown");
        if (!strftime(clock, sizeof(clock), "%H:%M:%S", &local))
            snprintf(clock, sizeof(clock), "Unknown");
    }

    border(out, 0xcd, 0xc9, 0, 0xbb);
    full_row(out, "                         System Configuration");
    border(out, 0xcd, 0xcc, 0, 0xb9);
    char board_row[300];
    snprintf(board_row, sizeof(board_row), "Board      : %s", board);
    full_row(out, board_row);
    border(out, 0xcd, 0xcc, 0xcb, 0xb9);
    pair_row(out, "Processor", cpu, "System", "Pi-DOS " PI_DOS_VERSION);
    pair_row(out, "CPU cores", cores, "DOS CPU", cpu_model);
    pair_row(out, "Host RAM", ram, "DOS memory", summary);
    pair_row(out, "Storage", disk, "DOS video", graphics);
    pair_row(out, "Access", !mounted ? "Unavailable" : writable ? "Read/write" : "Read-only",
             "DOS Voodoo", voodoo_status);
    pair_row(out, "Date", date, "DOS sound", sound);
    pair_row(out, "Time", clock, "DOS MIDI", midi);
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
    char regional[512];
    snprintf(regional, sizeof(regional), "Regional   : Keyboard %s / Country %s / %s", keyboard, country, zone);
    full_row(out, regional);
    border(out, 0xcd, 0xc8, 0, 0xbc);
    fputs("ECHO.\nECHO Welcome to Pi-DOS\nECHO.\n", out);
}
