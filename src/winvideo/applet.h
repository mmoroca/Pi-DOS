#ifndef PI_DOS_APPLET_H
#define PI_DOS_APPLET_H
/* Install a prepared PIDOS.NEW, or remove the current and legacy applets.
 * Returns 1 on success, 0 after rollback, -1 if rollback needs recovery.
 * Existing .OLD recovery files are never discarded to start another change. */
int pv_applet_commit(const char *system_directory, int uninstall);
#endif
