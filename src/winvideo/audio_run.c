/* DOS handoff to the unchanged vendor installer. The appliance owns BLASTER
 * and startup commands, so restore root CONFIG.SYS/AUTOEXEC.BAT afterwards.
 * Keep recovery copies if installation or restoration is interrupted.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <direct.h>
#include <process.h>
#include <errno.h>

static char buffer[4096];
static const char *roots[] = {"C:\\AUTOEXEC.BAT", "C:\\CONFIG.SYS"};
static const char *copies[] = {"AUTOEXEC.PVB", "CONFIG.PVB"};

static int copy_file(const char *source, const char *target)
{
    FILE *input = fopen(source, "rb"), *output;
    unsigned count;
    int ok = 1;
    if (!input) return 0;
    output = fopen(target, "wb");
    if (!output) { fclose(input); return 0; }
    while ((count = fread(buffer, 1, sizeof(buffer), input)) != 0)
        if (fwrite(buffer, 1, count, output) != count) { ok = 0; break; }
    if (ferror(input)) ok = 0;
    if (fclose(output)) ok = 0;
    fclose(input);
    return ok;
}

int main(int argc, char **argv)
{
    char directory[144], *slash;
    unsigned char present[2];
    FILE *file;
    int i, result, restored = 1;
    (void)argc;
    if (strlen(argv[0]) >= sizeof(directory)) return 1;
    strcpy(directory, argv[0]); slash = strrchr(directory, '\\');
    if (!slash) {
        puts("Run PVRUN.EXE using its full path, for example C:\\WINDOWS\\PIDOSAUD\\GUS\\PVRUN.EXE.");
        return 1;
    }
    *slash = 0;
    if (directory[1] == ':' && _chdrive(directory[0] & 31)) return 1;
    if (chdir(directory)) return 1;
    file = fopen("ROOTBACK.TAG", "rb");
    if (file) {
        fread(present, 1, sizeof(present), file); fclose(file);
        puts("An interrupted audio installation has startup-file backups here.");
        for (i = 0; i < 2; ++i) {
            if (present[i]) printf("Restore %s to %s.\n", copies[i], roots[i]);
            else printf("%s did not exist before installation.\n", roots[i]);
        }
        puts("Recover the startup files before removing ROOTBACK.TAG and retrying.");
        return 1;
    }
    for (i = 0; i < 2; ++i) {
        file = fopen(roots[i], "rb");
        present[i] = file != NULL;
        if (file) fclose(file);
        else if (errno != ENOENT) { puts("Cannot read the Pi-DOS startup files."); return 1; }
        if (present[i] && !copy_file(roots[i], copies[i])) {
            puts("Cannot back up the Pi-DOS startup files. Installer was not started."); return 1;
        }
    }
    file = fopen("ROOTBACK.TAG", "wb");
    if (!file) return 1;
    result = fwrite(present, 1, sizeof(present), file) == sizeof(present);
    if (fclose(file)) result = 0;
    if (!result) return 1;
    puts("Starting the original Gravis UltraSound installer. Match the Pi-DOS I/O, IRQ and DMA settings.");
    result = spawnl(P_WAIT, "INSTALL.EXE", "INSTALL.EXE", NULL);
    for (i = 0; i < 2; ++i) {
        if (present[i]) {
            if (!copy_file(copies[i], roots[i])) restored = 0;
        } else if (remove(roots[i]) && errno != ENOENT) restored = 0;
    }
    if (restored) remove("ROOTBACK.TAG");
    else {
        puts("Startup-file restoration failed. Recovery copies and ROOTBACK.TAG have been kept.");
        for (i = 0; i < 2; ++i) if (present[i]) printf("Restore %s to %s.\n", copies[i], roots[i]);
    }
    if (result == -1) puts("The original installer could not be started.");
    puts("Pi-DOS owns the DOS startup settings. Windows driver changes have been retained.");
    return restored && result != -1 ? 0 : 1;
}
