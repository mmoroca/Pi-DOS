/* Replace the applet only after Setup has checked that neither DLL is loaded.
 * Keep both old names recoverable until the replacement has been installed. */
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include "applet.h"

#ifdef PV_HOST
#define SEP "/"
#else
#define SEP "\\"
#endif

static int present(const char *path)
{
    struct stat info;
    return stat(path, &info) == 0;
}

int pv_applet_commit(const char *directory, int uninstall)
{
    static const char *names[] = {"PIDOS.CPL", "PIVIDEO.CPL"};
    static const char *backups[] = {"PIDOS.OLD", "PIVIDEO.OLD"};
    char target[144], backup[144], stage[144];
    int moved[2] = {0, 0}, i, ok = 1;
    if (strlen(directory) > 120) return 0;
    sprintf(stage, "%s" SEP "PIDOS.NEW", directory);
    if (!uninstall && !present(stage)) return 0;
    for (i = 0; i < 2; ++i) {
        sprintf(backup, "%s" SEP "%s", directory, backups[i]);
        if (present(backup)) return 0;
    }
    for (i = 0; i < 2; ++i) {
        sprintf(target, "%s" SEP "%s", directory, names[i]);
        sprintf(backup, "%s" SEP "%s", directory, backups[i]);
        if (present(target)) {
            if (rename(target, backup)) break;
            moved[i] = 1;
        }
    }
    if (i == 2 && !uninstall) {
        sprintf(target, "%s" SEP "PIDOS.CPL", directory);
        if (rename(stage, target)) i = 0;
    }
    if (i != 2) {
        for (i = 1; i >= 0; --i) if (moved[i]) {
            sprintf(target, "%s" SEP "%s", directory, names[i]);
            sprintf(backup, "%s" SEP "%s", directory, backups[i]);
            if (rename(backup, target)) ok = 0;
        }
        return ok ? 0 : -1;
    }
    for (i = 0; i < 2; ++i) if (moved[i]) {
        sprintf(backup, "%s" SEP "%s", directory, backups[i]);
        remove(backup);
    }
    return 1;
}
