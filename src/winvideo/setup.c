/* Native Windows 3.1 launcher; the CPL shares the installer/configuration UI. */
#include <windows.h>
#include <string.h>

typedef void (FAR PASCAL *InstallProc)(HWND);

int PASCAL WinMain(HINSTANCE instance, HINSTANCE previous, LPSTR command, int show)
{
    char path[144];
    char *slash;
    HINSTANCE library;
    InstallProc install;
    (void)previous; (void)command; (void)show;
    if (!GetModuleFileName(instance, path, sizeof(path))) return 1;
    slash = strrchr(path, '\\');
    if (!slash) return 1;
    strcpy(slash + 1, "PIVIDEO.CPL");
    library = LoadLibrary(path);
    if ((UINT)library < 32) {
        MessageBox(NULL, "Keep SETUP.EXE and PIVIDEO.CPL in the same folder.",
                   "Pi-DOS Display Setup", MB_OK | MB_ICONSTOP);
        return 1;
    }
    install = (InstallProc)GetProcAddress(library, "INSTALL");
    if (install) install(NULL);
    else MessageBox(NULL, "PIVIDEO.CPL does not contain the installer.",
                    "Pi-DOS Display Setup", MB_OK | MB_ICONSTOP);
    FreeLibrary(library);
    return install ? 0 : 1;
}
