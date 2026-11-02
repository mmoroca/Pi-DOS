/* Windows 3.1 native installer and Control Panel applet, built with Open Watcom.
 * Driver-specific configuration is generated from the supplied OEM manifests.
 * Never overwrite a loaded driver; all mode changes affect the next startup.
 */
#include <windows.h>
#include <stdio.h>
#include <string.h>
#include <direct.h>
#include <dos.h>
#include <errno.h>
#include <stdlib.h>
#include "video.h"
#include "transaction.h"

typedef struct { const char *file, *section, *key, *value; } PvOp;
typedef struct {
    const char *adapter, *description;
    int width, height, depth, dpi;
    const PvOp *ops;
    unsigned count;
} PvMode;
typedef struct { const char *adapter, *name; int system; unsigned long size; } PvFile;
#include "catalog.h"

/* Win16 Control Panel ABI (16-bit int/HICON, two-byte packing). */
#pragma pack(push, 2)
typedef struct { int idIcon, idName, idInfo; LONG lData; } PvCplInfo;
typedef struct {
    DWORD dwSize, dwFlags, dwHelpContext; LONG lData; HICON hIcon;
    char szName[32], szInfo[64], szHelpFile[128];
} PvNewCplInfo;
#pragma pack(pop)

static HINSTANCE module;
static int busy;
static char windir[144], sysdir[144], error_text[512];
static const char *ini_names[] = {"SYSTEM.INI", "WIN.INI", "PROGMAN.INI"};
static char io_buffer[4096], compare_buffer[4096];
static int selected_adapter, selected_width, selected_height, selected_depth, selected_dpi;
static const char *adapter_ids[] = {"S3", "ET4"};

static int path_join(char *out, const char *directory, const char *file)
{
    unsigned length = strlen(directory);
    if (length + strlen(file) + 2 > 144) {
        strcpy(error_text, "A Windows or installer path is too long.");
        return 0;
    }
    strcpy(out, directory);
    if (length && out[length - 1] != '\\') strcat(out, "\\");
    strcat(out, file);
    return 1;
}

static void report(HWND owner)
{
    MessageBox(owner, error_text, "Pi-DOS Display", MB_OK | MB_ICONSTOP);
}

static int exists(const char *path)
{
    FILE *file = fopen(path, "rb");
    if (!file) return 0;
    fclose(file);
    return 1;
}

static int copy_file(const char *from, const char *to)
{
    FILE *input, *output;
    unsigned count;
    int ok = 1;
    input = fopen(from, "rb");
    if (!input) goto fail;
    output = fopen(to, "wb");
    if (!output) { fclose(input); goto fail; }
    while ((count = fread(io_buffer, 1, sizeof(io_buffer), input)) != 0) {
        if (fwrite(io_buffer, 1, count, output) != count) { ok = 0; break; }
    }
    if (ferror(input)) ok = 0;
    if (fclose(output)) ok = 0;
    fclose(input);
    if (ok) return 1;
fail:
    sprintf(error_text, "Could not copy:\n%s\nto:\n%s\nCheck free space and write access.", from, to);
    return 0;
}

static int same_file(const char *a, const char *b)
{
    FILE *left = fopen(a, "rb"), *right = fopen(b, "rb");
    unsigned x, y;
    int equal = 0;
    if (left && right) {
        do {
            x = fread(io_buffer, 1, sizeof(io_buffer), left);
            y = fread(compare_buffer, 1, sizeof(compare_buffer), right);
            if (x != y || memcmp(io_buffer, compare_buffer, x)) break;
            if (!x) { equal = !ferror(left) && !ferror(right); break; }
        } while (1);
    }
    if (left) fclose(left);
    if (right) fclose(right);
    return equal;
}

static int ensure_directory(const char *name)
{
    unsigned attributes;
    int creation_errno, creation_dos_error;
    /* DOS AH=39h reports error 5 (access denied) for an existing directory,
     * which the Watcom runtime maps to EACCES, not EEXIST. Check its type
     * explicitly; a regular file with the same name must remain an error. */
    if (!_dos_getfileattr(name, &attributes)) {
        if (attributes & _A_SUBDIR) return 1;
        sprintf(error_text, "A file already occupies the required directory path:\n%s", name);
        return 0;
    }
    if (!_mkdir(name)) return 1;
    creation_errno = errno;
    creation_dos_error = _doserrno;
    /* Also accept a directory created between the attribute check and MKDIR. */
    if (!_dos_getfileattr(name, &attributes) && (attributes & _A_SUBDIR)) return 1;
    sprintf(error_text, "Could not create directory:\n%s\nDOS error %d, runtime error %d.\n"
            "Check that the Windows directory is writable and the disk has free space.",
            name, creation_dos_error, creation_errno);
    return 0;
}

static int check_windows(HWND owner)
{
    DWORD version = GetVersion();
    if (LOBYTE(LOWORD(version)) != 3 || HIBYTE(LOWORD(version)) < 10 ||
            HIBYTE(LOWORD(version)) > 11 || !(GetWinFlags() & WF_ENHANCED)) {
        MessageBox(owner, "Run this program in Windows 3.1/3.11 or Windows for Workgroups,\n"
                   "using 386 Enhanced Mode.", "Pi-DOS Display", MB_OK | MB_ICONSTOP);
        return 0;
    }
    if (!GetWindowsDirectory(windir, sizeof(windir)) ||
            !GetSystemDirectory(sysdir, sizeof(sysdir)) || strlen(windir) > 100 || strlen(sysdir) > 100) {
        strcpy(error_text, "Could not locate the Windows directories, or their paths are too long.");
        report(owner);
        return 0;
    }
    return 1;
}

/* A directory is used for each snapshot so original filenames remain useful
 * from DOS. Never replace the first snapshot after it has been completed. */
static int snapshot(const char *folder, int first)
{
    char directory[144], source[144], target[144], marker[144];
    FILE *file;
    unsigned i;
    if (!path_join(directory, windir, folder) || !ensure_directory(directory) ||
            !path_join(marker, directory, "READY")) return 0;
    if (first && exists(marker)) return 1;
    remove(marker);
    for (i = 0; i < 3; ++i) {
        if (!path_join(source, windir, ini_names[i]) ||
                !path_join(target, directory, ini_names[i])) return 0;
        WritePrivateProfileString(NULL, NULL, NULL, source);
        if (exists(source)) {
            if (!copy_file(source, target)) return 0;
        } else remove(target);
    }
    file = fopen(marker, "wb");
    if (!file) goto fail;
    if (fclose(file)) goto fail;
    return 1;
fail:
    strcpy(error_text, "Could not complete the configuration backup.");
    return 0;
}

static int restore_snapshot(const char *folder)
{
    char directory[144], source[144], target[144], marker[144], stage[144];
    unsigned i;
    FILE *file;
    if (!path_join(directory, windir, folder) || !path_join(marker, directory, "READY")) return 0;
    if (!exists(marker)) { strcpy(error_text, "No completed previous-settings backup exists."); return 0; }
    if (!path_join(stage, windir, "PVIDTEMP") || !ensure_directory(stage)) return 0;
    for (i = 0; i < 3; ++i) {
        if (!path_join(source, directory, ini_names[i]) || !path_join(target, stage, ini_names[i])) return 0;
        WritePrivateProfileString(NULL, NULL, NULL, target);
        if (exists(source)) {
            if (!copy_file(source, target)) return 0;
        } else {
            file = fopen(target, "wb");
            if (!file) { strcpy(error_text, "Could not prepare restoration."); return 0; }
            if (fclose(file)) { strcpy(error_text, "Could not save restoration files."); return 0; }
        }
        WritePrivateProfileString(NULL, NULL, NULL, target);
    }
    if (pv_commit(windir, stage) != 1) {
        strcpy(error_text, "Could not restore settings. Exit Windows and run PVRESTOR.BAT.");
        return 0;
    }
    for (i = 0; i < 3; ++i) {
        path_join(target, windir, ini_names[i]);
        WritePrivateProfileString(NULL, NULL, NULL, target);
    }
    return 1;
}

static int verify_payload(const char *adapter)
{
    unsigned i;
    char path[144];
    FILE *file;
    long size;
    for (i = 0; i < FILE_COUNT; ++i) {
        if (strcmp(payloads[i].adapter, adapter)) continue;
        if (!path_join(path, payloads[i].system ? sysdir : windir, payloads[i].name)) return 0;
        file = fopen(path, "rb");
        if (!file) goto missing;
        fseek(file, 0, SEEK_END); size = ftell(file); fclose(file);
        /* Retain installed/localized Windows fonts and existing vendor INIs. */
        if (strstr(payloads[i].name, ".FON") || strstr(payloads[i].name, ".INI")) continue;
        if (size != payloads[i].size) goto missing;
    }
    return 1;
missing:
    sprintf(error_text, "A required driver file is missing or has the wrong size:\n%s\nRun Pi-DOS Display SETUP.EXE again.", path);
    return 0;
}

static int check_adapter(HWND owner, const PvMode *mode)
{
    char machine[48];
    GetPrivateProfileString("dosbox", "machine", "", machine, sizeof(machine), "C:\\SYSTEM\\PIDOS.CFG");
    if (*machine) {
        if ((!strcmp(mode->adapter, "ET4") && stricmp(machine, "svga_et4000")) ||
                (!strcmp(mode->adapter, "S3") && stricmp(machine, "svga_s3") && stricmp(machine, "svga_s3trio64"))) {
            MessageBox(owner, "This driver does not match the video card selected in PIDOSCFG.\n"
                       "Select that card in PIDOSCFG, save and reboot Pi-DOS first.",
                       "Pi-DOS Display", MB_OK | MB_ICONEXCLAMATION);
            return 0;
        }
    }
    return MessageBox(owner, "Apply these display settings for the next Windows startup?\n\n"
                      "The selected card must match the emulated card in PIDOSCFG.\n"
                      "Save your work, then exit and start Windows again after applying.",
                      mode->description, MB_OKCANCEL | MB_ICONQUESTION) == IDOK;
}

static int write_recovery(void)
{
    char path[144];
    FILE *file;
    if (!path_join(path, windir, "PVRESTOR.BAT")) return 0;
    file = fopen(path, "wb");
    if (!file) goto fail;
    fprintf(file, "@ECHO OFF\r\nREM Run from DOS after exiting Windows.\r\n"
            "IF NOT EXIST %s\\PVIDLAST\\READY GOTO MISSING\r\n"
            "ECHO Restoring settings from before the last display change.\r\n"
            "COPY %s\\PVIDLAST\\SYSTEM.INI %s\\SYSTEM.INI\r\n"
            "IF ERRORLEVEL 1 GOTO FAILED\r\n"
            "COPY %s\\PVIDLAST\\WIN.INI %s\\WIN.INI\r\n"
            "IF ERRORLEVEL 1 GOTO FAILED\r\n"
            "IF EXIST %s\\PVIDLAST\\PROGMAN.INI COPY %s\\PVIDLAST\\PROGMAN.INI %s\\PROGMAN.INI\r\n"
            "ECHO Settings restored. Start Windows normally.\r\nGOTO END\r\n"
            ":MISSING\r\nECHO No completed backup exists.\r\nGOTO END\r\n"
            ":FAILED\r\nECHO Restore failed. Check write access and free space.\r\n:END\r\n",
            windir, windir, windir, windir, windir, windir, windir, windir);
    if (fclose(file)) goto fail;
    return 1;
fail:
    strcpy(error_text, "Could not create the DOS recovery command PVRESTOR.BAT.");
    return 0;
}

static int apply_mode(const PvMode *mode)
{
    char directory[144], from[144], to[144];
    char current_value[80];
    const char *value;
    const PvOp *op;
    FILE *file;
    unsigned i;
    int result;
    int codepage;
    if (!verify_payload(mode->adapter) || !snapshot("PVIDBACK", 1) ||
            !snapshot("PVIDLAST", 0) || !write_recovery()) return 0;
    if (!path_join(directory, windir, "PVIDTEMP") || !ensure_directory(directory)) return 0;
    if (!path_join(from, windir, "SYSTEM.INI")) return 0;
    codepage = GetPrivateProfileInt("boot.description", "codepage", 437, from);
    for (i = 0; i < 3; ++i) {
        if (!path_join(from, windir, ini_names[i]) || !path_join(to, directory, ini_names[i])) return 0;
        if (exists(from)) {
            if (!copy_file(from, to)) return 0;
        } else {
            file = fopen(to, "wb");
            if (!file) { strcpy(error_text, "Cannot prepare temporary INI files."); return 0; }
            if (fclose(file)) { strcpy(error_text, "Cannot save temporary INI files."); return 0; }
        }
        WritePrivateProfileString(NULL, NULL, NULL, to);
    }
    for (i = 0; i < mode->count; ++i) {
        op = &mode->ops[i];
        if (!path_join(to, directory, op->file)) return 0;
        value = op->value;
        /* The OEM grabber lists contain alternative CP437 / CP850 fonts. */
        if (codepage == 850 && !stricmp(op->section, "386Enh")) {
            if (!stricmp(op->key, "CGA40WOA.FON")) value = "CGA40850.FON";
            if (!stricmp(op->key, "CGA80WOA.FON")) value = "CGA80850.FON";
            if (!stricmp(op->key, "EGA40WOA.FON")) value = "EGA40850.FON";
            if (!stricmp(op->key, "EGA80WOA.FON")) value = "EGA80850.FON";
        }
        /* Remove only S3's own multimedia hooks when switching to ET4000. */
        if (!strcmp(mode->adapter, "ET4") && !value && !stricmp(op->section, "drivers")) {
            GetPrivateProfileString(op->section, op->key, "", current_value, sizeof(current_value), to);
            if (!stricmp(op->key, "DCI") && stricmp(current_value, "RFMDCI") &&
                    stricmp(current_value, "RFMDCI.DRV")) continue;
            if (!stricmp(op->key, "VIDS.DRAW") && stricmp(current_value, "UDH.DLL")) continue;
        }
        if (!WritePrivateProfileString(op->section, op->key, value, to)) {
            strcpy(error_text, "Could not prepare the display settings. Original settings are unchanged.");
            return 0;
        }
    }
    for (i = 0; i < 3; ++i) {
        if (!path_join(from, directory, ini_names[i])) return 0;
        WritePrivateProfileString(NULL, NULL, NULL, from);
    }
    result = pv_commit(windir, directory);
    if (result != 1) {
        strcpy(error_text, result == 0 ? "Could not save the new settings. Previous settings were restored." :
               "Settings could not be saved or fully restored.\n"
               "Exit Windows and run PVRESTOR.BAT from your Windows directory.");
        return 0;
    }
    for (i = 0; i < 3; ++i) {
        path_join(to, windir, ini_names[i]);
        WritePrivateProfileString(NULL, NULL, NULL, to);
    }
    return 1;
}

static const PvMode *selected_mode(void)
{
    unsigned i;
    for (i = 0; i < MODE_COUNT; ++i) {
        if (!strcmp(modes[i].adapter, adapter_ids[selected_adapter]) &&
                modes[i].width == selected_width && modes[i].height == selected_height &&
                modes[i].depth == selected_depth && modes[i].dpi == selected_dpi) return &modes[i];
    }
    return NULL;
}

static const char *depth_label(int depth)
{
    switch (depth) {
    case 4: return "16 colours (4 bit)";
    case 8: return "256 colours (8 bit)";
    case 15: return "32,768 colours (15 bit)";
    case 16: return "65,536 colours (16 bit)";
    default: return "16.7 million colours (32 bit)";
    }
}

static void fill_choices(HWND dialog)
{
    unsigned i;
    int index, selected;
    char text[80];
    HWND combo;
    combo = GetDlgItem(dialog, IDC_RESOLUTION);
    SendMessage(combo, CB_RESETCONTENT, 0, 0);
    for (i = 0; i < MODE_COUNT; ++i) {
        if (strcmp(modes[i].adapter, adapter_ids[selected_adapter])) continue;
        sprintf(text, "%d x %d", modes[i].width, modes[i].height);
        index = (int)SendMessage(combo, CB_FINDSTRING, (WPARAM)-1, (LPARAM)(LPSTR)text);
        if (index == CB_ERR) {
            index = (int)SendMessage(combo, CB_ADDSTRING, 0, (LPARAM)(LPSTR)text);
            SendMessage(combo, CB_SETITEMDATA, index, i);
        }
    }
    selected = 0;
    for (index = 0; index < (int)SendMessage(combo, CB_GETCOUNT, 0, 0); ++index) {
        i = (unsigned)SendMessage(combo, CB_GETITEMDATA, index, 0);
        if (modes[i].width == selected_width && modes[i].height == selected_height) selected = index;
    }
    SendMessage(combo, CB_SETCURSEL, selected, 0);
    i = (unsigned)SendMessage(combo, CB_GETITEMDATA, selected, 0);
    selected_width = modes[i].width; selected_height = modes[i].height;
    combo = GetDlgItem(dialog, IDC_COLOURS);
    SendMessage(combo, CB_RESETCONTENT, 0, 0);
    for (i = 0; i < MODE_COUNT; ++i) {
        if (strcmp(modes[i].adapter, adapter_ids[selected_adapter]) ||
                modes[i].width != selected_width || modes[i].height != selected_height) continue;
        strcpy(text, depth_label(modes[i].depth));
        index = (int)SendMessage(combo, CB_FINDSTRING, (WPARAM)-1, (LPARAM)(LPSTR)text);
        if (index == CB_ERR) {
            index = (int)SendMessage(combo, CB_ADDSTRING, 0, (LPARAM)(LPSTR)text);
            SendMessage(combo, CB_SETITEMDATA, index, modes[i].depth);
        }
    }
    selected = 0;
    for (index = 0; index < (int)SendMessage(combo, CB_GETCOUNT, 0, 0); ++index)
        if ((int)SendMessage(combo, CB_GETITEMDATA, index, 0) == selected_depth) selected = index;
    SendMessage(combo, CB_SETCURSEL, selected, 0);
    selected_depth = (int)SendMessage(combo, CB_GETITEMDATA, selected, 0);
    combo = GetDlgItem(dialog, IDC_FONTS);
    SendMessage(combo, CB_RESETCONTENT, 0, 0);
    for (i = 0; i < MODE_COUNT; ++i) {
        if (strcmp(modes[i].adapter, adapter_ids[selected_adapter]) ||
                modes[i].width != selected_width || modes[i].height != selected_height ||
                modes[i].depth != selected_depth) continue;
        strcpy(text, modes[i].dpi == 120 ? "Large (LF, 120 dpi)" : "Small (SF, 96 dpi)");
        index = (int)SendMessage(combo, CB_ADDSTRING, 0, (LPARAM)(LPSTR)text);
        SendMessage(combo, CB_SETITEMDATA, index, modes[i].dpi);
    }
    selected = 0;
    for (index = 0; index < (int)SendMessage(combo, CB_GETCOUNT, 0, 0); ++index)
        if ((int)SendMessage(combo, CB_GETITEMDATA, index, 0) == selected_dpi) selected = index;
    SendMessage(combo, CB_SETCURSEL, selected, 0);
    selected_dpi = (int)SendMessage(combo, CB_GETITEMDATA, selected, 0);
    SetDlgItemText(dialog, IDC_STATUS, selected_adapter ?
        "ET4000 high colour requires the emulated Sierra RAMDAC.\n1024 x 768 supports up to 256 colours." :
        "S3 Trio64: available colours depend on resolution and video memory.");
}

BOOL FAR PASCAL __export VideoDialog(HWND dialog, UINT message, WPARAM wparam, LPARAM lparam)
{
    char text[128], ini[144], description[128], machine[48];
    HDC dc;
    unsigned i;
    int index;
    const PvMode *mode;
    if (message == WM_INITDIALOG) {
        dc = GetDC(dialog);
        selected_width = GetDeviceCaps(dc, HORZRES); selected_height = GetDeviceCaps(dc, VERTRES);
        selected_depth = GetDeviceCaps(dc, BITSPIXEL) * GetDeviceCaps(dc, PLANES);
        selected_dpi = GetDeviceCaps(dc, LOGPIXELSY);
        ReleaseDC(dialog, dc);
        sprintf(text, "Running: %d x %d, %s", selected_width, selected_height, depth_label(selected_depth));
        SetDlgItemText(dialog, IDC_CURRENT, text);
        path_join(ini, windir, "SYSTEM.INI");
        GetPrivateProfileString("boot.description", "display.drv", "", description, sizeof(description), ini);
        GetPrivateProfileString("dosbox", "machine", "", machine, sizeof(machine), "C:\\SYSTEM\\PIDOS.CFG");
        selected_adapter = !stricmp(machine, "svga_et4000") ? 1 : 0;
        for (i = 0; i < MODE_COUNT; ++i) {
            if (!strcmp(modes[i].description, description)) {
                selected_adapter = !strcmp(modes[i].adapter, "ET4");
                selected_width = modes[i].width; selected_height = modes[i].height;
                selected_depth = modes[i].depth; selected_dpi = modes[i].dpi;
                break;
            }
        }
        SendDlgItemMessage(dialog, IDC_ADAPTER, CB_ADDSTRING, 0, (LPARAM)(LPSTR)"S3 Trio64");
        SendDlgItemMessage(dialog, IDC_ADAPTER, CB_ADDSTRING, 0, (LPARAM)(LPSTR)"Tseng ET4000");
        SendDlgItemMessage(dialog, IDC_ADAPTER, CB_SETCURSEL, selected_adapter, 0);
        fill_choices(dialog);
        return TRUE;
    }
    if (message == WM_COMMAND) {
        if (wparam == IDCANCEL) { EndDialog(dialog, 0); return TRUE; }
        if (wparam == IDC_RESTORE) {
            if (MessageBox(dialog, "Restore the INI files from before the last display change?\n"
                           "This also restores other settings saved in those files at that time.",
                           "Restore previous settings", MB_OKCANCEL | MB_ICONQUESTION) == IDOK) {
                if (restore_snapshot("PVIDLAST")) {
                    MessageBox(dialog, "Previous settings restored. Exit and start Windows again.",
                               "Pi-DOS Display", MB_OK | MB_ICONINFORMATION);
                    EndDialog(dialog, 0);
                } else report(dialog);
            }
            return TRUE;
        }
        if (wparam == IDOK) {
            mode = selected_mode();
            if (mode && check_adapter(dialog, mode)) {
                if (apply_mode(mode)) {
                    MessageBox(dialog, "Settings saved. Exit and start Windows again to apply them.\n"
                               "If Windows cannot start, run PVRESTOR.BAT from your Windows directory in DOS.",
                               "Pi-DOS Display", MB_OK | MB_ICONINFORMATION);
                    EndDialog(dialog, 1);
                } else report(dialog);
            }
            return TRUE;
        }
        if (HIWORD(lparam) == CBN_SELCHANGE) {
            index = (int)SendDlgItemMessage(dialog, wparam, CB_GETCURSEL, 0, 0);
            if (index == CB_ERR) return TRUE;
            if (wparam == IDC_ADAPTER) selected_adapter = index;
            else if (wparam == IDC_RESOLUTION) {
                i = (unsigned)SendDlgItemMessage(dialog, wparam, CB_GETITEMDATA, index, 0);
                selected_width = modes[i].width; selected_height = modes[i].height;
            } else if (wparam == IDC_COLOURS)
                selected_depth = (int)SendDlgItemMessage(dialog, wparam, CB_GETITEMDATA, index, 0);
            else if (wparam == IDC_FONTS)
                selected_dpi = (int)SendDlgItemMessage(dialog, wparam, CB_GETITEMDATA, index, 0);
            fill_choices(dialog);
            return TRUE;
        }
    }
    return FALSE;
}

static void show_dialog(HWND owner)
{
    if (busy || !check_windows(owner)) return;
    busy = 1;
    DialogBox(module, MAKEINTRESOURCE(IDD_VIDEO), owner, VideoDialog);
    busy = 0;
}

void FAR PASCAL __export Install(HWND owner)
{
    char source_dir[144], adapter_dir[144], source[144], target[144];
    char *slash;
    unsigned i;
    FILE *file;
    long size;
    if (busy || !check_windows(owner)) return;
    if (MessageBox(owner, "Install the S3 Trio64 and Tseng ET4000 drivers and add\n"
                   "Pi-DOS Display to Control Panel?\n\nExisting Windows font files will be kept.",
                   "Pi-DOS Display Setup", MB_OKCANCEL | MB_ICONQUESTION) != IDOK) return;
    if (!GetModuleFileName(module, source_dir, sizeof(source_dir))) return;
    slash = strrchr(source_dir, '\\');
    if (!slash) return;
    *slash = 0;
    /* Preflight every source and every existing non-font binary before copying. */
    for (i = 0; i < FILE_COUNT; ++i) {
        if (!path_join(adapter_dir, source_dir, payloads[i].adapter) ||
                !path_join(source, adapter_dir, payloads[i].name) ||
                !path_join(target, payloads[i].system ? sysdir : windir, payloads[i].name)) { report(owner); return; }
        file = fopen(source, "rb");
        if (!file) goto missing;
        fseek(file, 0, SEEK_END); size = ftell(file); fclose(file);
        if (size != payloads[i].size) goto missing;
        if (exists(target) && !strstr(payloads[i].name, ".FON") &&
                !strstr(payloads[i].name, ".INI") && !same_file(source, target)) {
            sprintf(error_text, "A different driver file already exists:\n%s\n"
                    "Install matching vendor drivers through Windows Setup before retrying.\n"
                    "Pi-DOS Display will not overwrite a driver in use.", target);
            report(owner); return;
        }
    }
    if (!path_join(source, source_dir, "PIVIDEO.CPL") || !path_join(target, sysdir, "PIVIDEO.CPL")) { report(owner); return; }
    if (exists(target) && !same_file(source, target)) {
        strcpy(error_text, "A different PIVIDEO.CPL is already installed.\n"
               "Close Control Panel and remove the old applet from DOS before installing this version.");
        report(owner); return;
    }
    for (i = 0; i < FILE_COUNT; ++i) {
        path_join(adapter_dir, source_dir, payloads[i].adapter);
        path_join(source, adapter_dir, payloads[i].name);
        path_join(target, payloads[i].system ? sysdir : windir, payloads[i].name);
        if (!exists(target) && !copy_file(source, target)) { report(owner); return; }
    }
    path_join(source, source_dir, "PIVIDEO.CPL"); path_join(target, sysdir, "PIVIDEO.CPL");
    if (!exists(target) && !copy_file(source, target)) { report(owner); return; }
    show_dialog(owner);
    return;
missing:
    sprintf(error_text, "A driver file is missing or has the wrong size:\n%s\n"
            "Extract the complete Pi-DOS Display package and retry.", source);
    report(owner);
}

LONG FAR PASCAL __export CPlApplet(HWND owner, UINT message, LONG param1, LONG param2)
{
    PvCplInfo FAR *info;
    PvNewCplInfo FAR *new_info;
    (void)param1;
    switch (message) {
    case 1: return 1; /* CPL_INIT */
    case 2: return 1; /* CPL_GETCOUNT */
    case 3: /* CPL_INQUIRE */
        info = (PvCplInfo FAR *)param2;
        info->idIcon = IDI_VIDEO; info->idName = IDS_NAME; info->idInfo = IDS_INFO; info->lData = 0;
        return 0;
    case 5: show_dialog(owner); return 0; /* CPL_DBLCLK */
    case 8: /* CPL_NEWINQUIRE: required by Windows 3.1 */
        new_info = (PvNewCplInfo FAR *)param2;
        _fmemset(new_info, 0, sizeof(*new_info));
        new_info->dwSize = sizeof(*new_info);
        new_info->hIcon = LoadIcon(module, MAKEINTRESOURCE(IDI_VIDEO));
        _fstrcpy(new_info->szName, "Pi-DOS Display");
        _fstrcpy(new_info->szInfo, "S3 and ET4000 resolution, colours and fonts");
        return 0;
    }
    return 0;
}

int FAR PASCAL LibMain(HINSTANCE instance, WORD data, WORD heap, LPSTR command)
{
    (void)data; (void)heap; (void)command;
    module = instance;
    return 1;
}

int FAR PASCAL WEP(int reason)
{
    (void)reason;
    return 1;
}
