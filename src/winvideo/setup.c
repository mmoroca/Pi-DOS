/* Load the installer under its own module name, never through an installed CPL. */
#include <windows.h>
#include <string.h>
#include "video.h"

typedef int (FAR PASCAL *InstallFromProc)(HWND, LPCSTR);
typedef int (FAR PASCAL *UninstallProc)(HWND);

static char source_dir[144];
static InstallFromProc install_from;
static UninstallProc uninstall_applet;

BOOL FAR PASCAL __export SetupDialog(HWND dialog, UINT message, WPARAM wparam, LPARAM lparam)
{
    int result;
    (void)lparam;
    if (message == WM_INITDIALOG) return TRUE;
    if (message == WM_COMMAND) {
        if (wparam == IDCANCEL) { EndDialog(dialog, 0); return TRUE; }
        if (wparam == IDOK || wparam == IDC_UNINSTALL) {
            EnableWindow(dialog, FALSE);
            result = wparam == IDOK ? install_from(dialog, source_dir) : uninstall_applet(dialog);
            EnableWindow(dialog, TRUE);
            if (result) EndDialog(dialog, 1);
            return TRUE;
        }
    }
    return FALSE;
}

int PASCAL WinMain(HINSTANCE instance, HINSTANCE previous, LPSTR command, int show)
{
    char path[144];
    char *slash;
    HINSTANCE library;
    DLGPROC callback;
    int result;
    (void)previous; (void)show;
    if (GetModuleHandle("PIDINST")) {
        MessageBox(NULL, "Pi-DOS Setup is already running. Close it before starting another copy.",
                   "Pi-DOS Configuration Setup", MB_OK | MB_ICONEXCLAMATION);
        return 1;
    }
    if (!GetModuleFileName(instance, source_dir, sizeof(source_dir))) return 1;
    slash = strrchr(source_dir, '\\');
    if (!slash) return 1;
    *slash = 0;
    if (strlen(source_dir) + sizeof("\\PIDINST.DLL") > sizeof(path)) return 1;
    strcpy(path, source_dir); strcat(path, "\\PIDINST.DLL");
    library = LoadLibrary(path);
    if ((UINT)library < 32) {
        MessageBox(NULL, "Keep SETUP.EXE, PIDINST.DLL and PIDOS.CPL in the same folder.",
                   "Pi-DOS Configuration Setup", MB_OK | MB_ICONSTOP);
        return 1;
    }
    install_from = (InstallFromProc)GetProcAddress(library, "INSTALLFROM");
    uninstall_applet = (UninstallProc)GetProcAddress(library, "UNINSTALL");
    result = 0;
    if (install_from && uninstall_applet) {
        if (!stricmp(command, "/uninstall") || !stricmp(command, "-uninstall"))
            result = uninstall_applet(NULL);
        else {
            callback = (DLGPROC)MakeProcInstance((FARPROC)SetupDialog, instance);
            if (callback) {
                result = DialogBox(instance, MAKEINTRESOURCE(IDD_SETUP), NULL, callback);
                FreeProcInstance((FARPROC)callback);
            }
        }
    } else MessageBox(NULL, "PIDINST.DLL does not contain the required installer entry points.",
                      "Pi-DOS Configuration Setup", MB_OK | MB_ICONSTOP);
    FreeLibrary(library);
    return result == 1 ? 0 : 1;
}
