/* Install existing vendor drivers from the shared native Windows frontend. */
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "video.h"
#include "config.h"
#include "transaction.h"
#include "audio_ini.h"

typedef struct { const char *name; unsigned long size; } AudioFile;
typedef struct {
    const char *id, *label;
    int card, dos;
    unsigned first, count;
    const char *wave, *fm, *aux;
} AudioProfile;
#include "audio_catalog.h"

static int audio_directory(char *out, const char *id)
{
    char base[144];
    return pv_path(base, pv_windows(), "PIDOSAUD") && pv_path(out, base, !strcmp(id, "SB16") ? "SB16WU" : id);
}

static int exists(const char *path)
{
    FILE *file = fopen(path, "rb");
    if (!file) return 0;
    fclose(file); return 1;
}

static void explain(HWND owner, const char *text)
{
    MessageBox(owner, text, "Pi-DOS audio", MB_OK | MB_ICONINFORMATION);
}

static int verify(const char *folder, const AudioProfile *profile, int helper)
{
    char path[144];
    FILE *file;
    long length;
    unsigned i;
    const AudioFile *item;
    for (i = 0; i < profile->count + (helper && profile->dos); ++i) {
        item = i < profile->count ? &audio_files[profile->first + i] : NULL;
        if (!pv_path(path, folder, item ? item->name : "PVRUN.EXE")) return 0;
        file = fopen(path, "rb");
        if (!file) return 0;
        fseek(file, 0, SEEK_END); length = ftell(file); fclose(file);
        if (length <= 0 || (item && length != item->size)) return 0;
    }
    return 1;
}

int pv_stage_audio(HWND owner, const char *source)
{
    char from_base[144], from_dir[144], to_dir[144], base[144], from[144], to[144];
    unsigned i, j;
    const AudioProfile *profile;
    const char *name;
    if (!pv_path(from_base, source, "AUDIO") || !pv_path(base, pv_windows(), "PIDOSAUD")) goto fail;
    for (j = 0; j < AUDIO_PROFILE_COUNT; ++j) {
        profile = &audio_profiles[j];
        if (!pv_path(from_dir, from_base, profile->id) || !verify(from_dir, profile, 1)) {
            explain(owner, "An audio driver package is incomplete. Extract the complete AUDIO directory before running Setup.");
            return 0;
        }
    }
    if (!pv_directory(base)) goto fail;
    for (j = 0; j < AUDIO_PROFILE_COUNT; ++j) {
        profile = &audio_profiles[j];
        if (!pv_path(from_dir, from_base, profile->id) || !audio_directory(to_dir, profile->id) ||
                !pv_directory(to_dir)) goto fail;
        for (i = 0; i < profile->count + profile->dos; ++i) {
            name = i == profile->count ? "PVRUN.EXE" : audio_files[profile->first + i].name;
            if (!pv_path(from, from_dir, name) || !pv_path(to, to_dir, name)) goto fail;
            /* The DOS handoff is our installer helper, never a loaded Windows
             * driver. Allow Setup to update it when reinstalling the app. */
            if (exists(to) && !pv_same(from, to) && strcmp(name, "PVRUN.EXE")) {
                explain(owner, "A different audio package is already cached. Exit Windows and move the old PIDOSAUD card folder aside before upgrading.");
                return 0;
            }
            if (!pv_same(from, to) && !pv_copy(from, to)) goto fail;
        }
    }
    return 1;
fail:
    pv_report(owner); return 0;
}

static long blaster_resource(const char *blaster, int resource, int base)
{
    const char *p = blaster;
    char *end;
    int key;
    long value;
    if (!p) return -1;
    while (*p) {
        while (isspace((unsigned char)*p)) ++p;
        if (!*p) break;
        key = toupper((unsigned char)*p++);
        value = strtol(p, &end, base);
        if (key == resource) return end == p ? -1 : value;
        while (*p && !isspace((unsigned char)*p)) ++p;
    }
    return -1;
}

static int running_card(HWND owner, const AudioProfile *profile)
{
    const char *blaster = getenv("BLASTER"), *ultra = getenv("ULTRASND");
    unsigned port, dma, dma2, irq, irq2;
    int type;
    if (!cfg_load("C:\\SYSTEM\\PIDOS.CFG")) {
        explain(owner, "Cannot read C:\\SYSTEM\\PIDOS.CFG."); return 0;
    }
    if (profile->card == -1) {
        if (!stricmp(cfg_get(K_SPEAKER), "true")) return 1;
        explain(owner, "Enable PC speaker in Pi-DOS settings, save and reboot Pi-DOS first."); return 0;
    }
    if (cfg_card() != profile->card) goto reboot;
    if (profile->card == CARD_GUS) {
        if (!ultra || sscanf(ultra, "%x,%u,%u,%u,%u", &port, &dma, &dma2, &irq, &irq2) != 5 ||
                port != strtol(cfg_get(K_GUS_PORT), NULL, 16) || dma != atol(cfg_get(K_GUS_DMA)) ||
                dma2 != dma || irq != atol(cfg_get(K_GUS_IRQ)) || irq2 != irq ||
                !getenv("ULTRADIR") || stricmp(getenv("ULTRADIR"), cfg_get(K_GUS_DIR))) goto reboot;
    } else {
        type = profile->card == CARD_SB16 ? 6 : profile->card == CARD_PRO2 ? 5 : 3;
        if (blaster_resource(blaster, 'T', 10) != type ||
                blaster_resource(blaster, 'A', 16) != strtol(cfg_get(K_SB_PORT), NULL, 16) ||
                blaster_resource(blaster, 'I', 10) != atol(cfg_get(K_SB_IRQ)) ||
                blaster_resource(blaster, 'D', 10) != atol(cfg_get(K_SB_DMA)) ||
                (type == 6 && blaster_resource(blaster, 'H', 10) != atol(cfg_get(K_SB_HDMA)))) goto reboot;
    }
    return 1;
reboot:
    explain(owner, "The selected driver must match the running Pi-DOS sound card and resources.\n"
            "Choose the card in Pi-DOS sound settings, save, exit Windows and reboot Pi-DOS first.");
    return 0;
}

static void driver_configure(HWND owner, const char *alias)
{
    HDRVR driver;
    DRVCONFIGINFO info;
    driver = OpenDriver(alias, "drivers", 0);
    if (!driver) { explain(owner, "This Windows driver could not be loaded. Install it and restart Windows first."); return; }
    info.dwDCISize = sizeof(info); info.lpszDCISectionName = "drivers"; info.lpszDCIAliasName = alias;
    if (SendDriverMessage(driver, DRV_QUERYCONFIGURE, 0, 0)) {
        if (pv_backup()) SendDriverMessage(driver, DRV_CONFIGURE, (LPARAM)owner, (LPARAM)&info);
        else pv_report(owner);
    } else explain(owner, "This driver does not provide a configuration dialog.");
    CloseDriver(driver, 0, 0);
}

static int ini(const char *file, const char *section, const char *key, const char *value)
{
    return WritePrivateProfileString(section, key, value, file) != 0;
}

/* The aliases and resource keys below are documented by Creative's README.
 * VSBPD versions remain in separate cache folders to avoid overwriting a VxD. */
static int alias(const char *file, const char *key, const char *folder, const char *name, const char *label)
{
    if (!*name) return 1;
    (void)folder;
    return ini(file, "drivers", key, name) && ini(file, "drivers.desc", name, label);
}

static int known_audio(const char *value)
{
    unsigned i;
    const char *name = strrchr(value, '\\');
    name = name ? name + 1 : value;
    if (!stricmp(name, "MSADLIB.DRV") || !stricmp(name, "SNDBLST.DRV") ||
            !stricmp(name, "ULTRASND.DRV")) return 1;
    for (i = 0; i < AUDIO_PROFILE_COUNT; ++i)
        if (!stricmp(name, audio_profiles[i].wave) ||
                (*audio_profiles[i].fm && !stricmp(name, audio_profiles[i].fm)) ||
                (*audio_profiles[i].aux && !stricmp(name, audio_profiles[i].aux))) return 1;
    return 0;
}

static void install_native(HWND owner, const AudioProfile *profile, const char *folder)
{
    static const char *names[] = {"SYSTEM.INI", "WIN.INI", "PROGMAN.INI"};
    char stage[144], from[144], to[144], system[144], temporary[144], vxd[144];
    static char keys[2048], value[512];
    char *key, *next;
    const char *name;
    unsigned i, pass;
    const AudioFile *item;
    const char *ext;
    FILE *file;
    int result;
    if (!pv_path(system, pv_windows(), "SYSTEM.INI")) goto failed;
    /* Windows supplies these common multimedia components. Never register a
     * nonexistent system driver or try to redistribute the Windows disks. */
    for (i = 0; i < 4; ++i) {
        name = i == 0 ? "TIMER.DRV" : i == 1 ? "MIDIMAP.DRV" : i == 2 ? "MCIWAVE.DRV" : "MCISEQ.DRV";
        if (!pv_path(from, pv_system(), name) || !exists(from)) {
            explain(owner, "Install Windows' Timer, MIDI Mapper, [MCI] Sound and [MCI] MIDI Sequencer components using Control Panel / Drivers first. Windows may request its installation disks."); return;
        }
    }
    /* Follow the original OEM layout for DLL lookup and the Pro mixer.
     * Refuse incompatible installed files before changing any configuration. */
    for (i = 0; i < profile->count; ++i) {
        item = &audio_files[profile->first + i]; ext = strrchr(item->name, '.');
        if (ext && !stricmp(ext, ".DRV") && stricmp(item->name, profile->wave) &&
                stricmp(item->name, profile->fm) && stricmp(item->name, profile->aux)) continue;
        if (!ext || (stricmp(ext, ".DRV") && stricmp(ext, ".DLL") &&
                stricmp(item->name, "SBPMIXER.EXE"))) continue;
        if (!pv_path(from, folder, item->name) || !pv_path(system, pv_system(), item->name)) goto failed;
        if (exists(system) && !pv_same(from, system)) {
            explain(owner, "A different version of this audio driver is already installed. Use Windows Drivers / Add / Unlisted with the staged PIDOSAUD driver folder to perform a vendor upgrade."); return;
        }
    }
    sprintf(value, "Install %s for the selected Pi-DOS card?\n\nWindows audio aliases will be updated. Save your work and restart Windows after installation.", profile->label);
    if (MessageBox(owner, value, "Install selected audio driver", MB_OKCANCEL | MB_ICONQUESTION) != IDOK) return;
    if (!pv_backup()) { pv_report(owner); return; }
    if (!pv_path(stage, pv_windows(), "PVIDTEMP") || !pv_directory(stage)) goto failed;
    for (i = 0; i < 3; ++i) {
        if (!pv_path(from, pv_windows(), names[i]) || !pv_path(to, stage, names[i])) goto failed;
        if (exists(from)) { if (!pv_copy(from, to)) goto failed; }
        else { file = fopen(to, "wb"); if (!file) goto failed; if (fclose(file)) goto failed; }
        WritePrivateProfileString(NULL, NULL, NULL, to);
    }
    if (!pv_path(to, stage, "SYSTEM.INI") || !pv_path(temporary, stage, "AUDIO.INI")) goto failed;
    *vxd = 0;
    if (*profile->fm && !pv_path(vxd, folder, "VSBPD.386")) goto failed;
    if (!pa_sound_vxd(to, temporary, vxd) || !pv_copy(temporary, to)) goto failed;
    WritePrivateProfileString(NULL, NULL, NULL, to);
    if (GetPrivateProfileString("drivers", NULL, "", keys, sizeof(keys), to) >= sizeof(keys) - 2) goto failed;
    for (key = keys; *key; key = next) {
        next = key + strlen(key) + 1;
        GetPrivateProfileString("drivers", key, "", value, sizeof(value), to);
        if (known_audio(value) && !ini(to, "drivers", key, NULL)) goto failed;
    }
    if (!alias(to, "wave", folder, profile->wave, profile->label) ||
            !alias(to, "midi", folder, profile->fm, profile->label) ||
            !alias(to, "midi1", folder, *profile->fm ? profile->wave : "", profile->label) ||
            !alias(to, "aux", folder, profile->aux, profile->label) ||
            !ini(to, "drivers", "timer", "TIMER.DRV") || !ini(to, "drivers", "midimapper", "MIDIMAP.DRV") ||
            !ini(to, "mci", "WaveAudio", "MCIWAVE.DRV") || !ini(to, "mci", "Sequencer", "MCISEQ.DRV")) goto failed;
    if (*profile->fm && (!ini(to, "sndblst.drv", "port", cfg_get(K_SB_PORT)) ||
            !ini(to, "sndblst.drv", "int", cfg_get(K_SB_IRQ)) ||
            !ini(to, "sndblst.drv", "dmachannel", cfg_get(K_SB_DMA)))) goto failed;
    if (profile->card == CARD_SB16 &&
            (!ini(to, "sndblst.drv", "hdmachannel", cfg_get(K_SB_HDMA)) ||
             !ini(to, "sndblst.drv", "midiport", cfg_get(K_MPU_BASE)))) goto failed;
    /* Copy support libraries first, then drivers. No existing different file
     * is replaced, including on a repeat install or after a failed copy. */
    for (pass = 0; pass < 2; ++pass) for (i = 0; i < profile->count; ++i) {
        item = &audio_files[profile->first + i]; ext = strrchr(item->name, '.');
        if (ext && !stricmp(ext, ".DRV") && stricmp(item->name, profile->wave) &&
                stricmp(item->name, profile->fm) && stricmp(item->name, profile->aux)) continue;
        if (!ext || (stricmp(ext, ".DRV") && stricmp(ext, ".DLL") &&
                stricmp(item->name, "SBPMIXER.EXE"))) continue;
        if ((!stricmp(ext, ".DRV")) != (pass == 1)) continue;
        if (!pv_path(from, folder, item->name) || !pv_path(system, pv_system(), item->name)) goto failed;
        if (!pv_same(from, system) && !pv_copy(from, system)) goto failed;
    }
    /* Keep any existing MIDI maps. The mapper can be edited through its own
     * dialog; the original map also remains on the staged OEM driver disk. */
    if (!pv_path(from, folder, "MIDIMAP.CFG") || !pv_path(system, pv_system(), "MIDIMAP.CFG")) goto failed;
    if (exists(from) && !exists(system) && !pv_copy(from, system)) goto failed;
    for (i = 0; i < 3; ++i) {
        pv_path(from, stage, names[i]); WritePrivateProfileString(NULL, NULL, NULL, from);
    }
    result = pv_commit(pv_windows(), stage);
    if (result != 1) goto failed;
    for (i = 0; i < 3; ++i) {
        pv_path(from, pv_windows(), names[i]); WritePrivateProfileString(NULL, NULL, NULL, from);
    }
    explain(owner, "The selected driver is installed. Exit and restart Windows to activate it.\n\nExisting MIDI maps were preserved. After restarting, use Windows MIDI Mapper to choose or create the required FM or external MIDI mapping.");
    return;
failed:
    explain(owner, "Could not complete audio installation. Exit Windows and run WINDOWS\\PVRESTOR.BAT to restore the saved INIs if needed. Check free disk space and write access.");
}

/* Open the actual Windows 3.1 Drivers applet, inside this application. */
static void windows_drivers(HWND owner)
{
    typedef LONG (FAR PASCAL *Applet)(HWND, UINT, LPARAM, LPARAM);
    HINSTANCE library;
    Applet applet;
    char path[144];
    if (!pv_path(path, pv_system(), "DRIVERS.CPL")) return;
    library = LoadLibrary(path);
    if (library < HINSTANCE_ERROR) { explain(owner, "Cannot load Windows' DRIVERS.CPL from WINDOWS\\SYSTEM."); return; }
    applet = (Applet)GetProcAddress(library, "CPLAPPLET");
    if (applet && applet(owner, 1, 0, 0)) {
        if (applet(owner, 2, 0, 0) > 0) { applet(owner, 5, 0, 0); applet(owner, 6, 0, 0); }
        applet(owner, 7, 0, 0);
    } else explain(owner, "Windows' Drivers Control Panel applet is unavailable.");
    FreeLibrary(library);
}

static void audio_info(HWND dialog)
{
    char text[400];
    int card;
    if (!cfg_load("C:\\SYSTEM\\PIDOS.CFG")) { SetDlgItemText(dialog, IDC_AUDIO_INFO, "Pi-DOS configuration is unavailable."); return; }
    card = cfg_card();
    if (card == CARD_GUS)
        sprintf(text, "Saved card: %s\nI/O %sh, IRQ %s, DMA %s, RAM %s KB\nDirectory: %s\nReboot Pi-DOS after changing emulated hardware.", card_names[card], cfg_get(K_GUS_PORT), cfg_get(K_GUS_IRQ), cfg_get(K_GUS_DMA), cfg_get(K_GUS_RAM), cfg_get(K_GUS_DIR));
    else
        sprintf(text, "Saved card: %s\nI/O %sh, IRQ %s, DMA %s, high DMA %s\nMIDI interface: %sh\nReboot Pi-DOS after changing emulated hardware.", card < 0 ? "Custom" : card_names[card], cfg_get(K_SB_PORT), cfg_get(K_SB_IRQ), cfg_get(K_SB_DMA), cfg_get(K_SB_HDMA), cfg_get(K_MPU_BASE));
    SetDlgItemText(dialog, IDC_AUDIO_INFO, text);
}

static void install_selected(HWND dialog)
{
    char folder[144], installer[144], text[512];
    unsigned selected = (unsigned)SendDlgItemMessage(dialog, IDC_AUDIO_CARD, CB_GETCURSEL, 0, 0);
    const AudioProfile *profile;
    if (selected >= AUDIO_PROFILE_COUNT) {
        if (selected == AUDIO_PROFILE_COUNT) {
            if (!cfg_load("C:\\SYSTEM\\PIDOS.CFG") || cfg_card() != CARD_ADLIB) {
                explain(dialog, "Select AdLib in Pi-DOS sound settings, save and reboot Pi-DOS first."); return;
            }
            explain(dialog, "In Windows Drivers, choose Add, then Ad Lib. Windows may request its installation disks. AdLib provides FM MIDI music only.");
            if (pv_backup()) windows_drivers(dialog); else pv_report(dialog);
        } else explain(dialog, "No confirmed compatible Windows 3.x driver has been located for this card. Its DOS emulation remains available.");
        return;
    }
    profile = &audio_profiles[selected];
    if (!running_card(dialog, profile)) return;
    if (!audio_directory(folder, profile->id) || !verify(folder, profile, 1)) {
        explain(dialog, "Run the current Pi-DOS SETUP.EXE with its complete AUDIO directory to stage this card's driver package first."); return;
    }
    if (!profile->dos) { install_native(dialog, profile, folder); return; }
    if (!pv_path(installer, folder, "PVRUN.EXE")) { pv_report(dialog); return; }
    sprintf(text, "Windows will close and run Gravis' original 4.11 installer.\n"
            "Install to %s using I/O %sh, IRQ %s and DMA %s.\n"
            "Choose Custom Setup and update Windows in %s.\n"
            "Do not install SBOS or Mega-Em. Save your work first.",
            cfg_get(K_GUS_DIR), cfg_get(K_GUS_PORT), cfg_get(K_GUS_IRQ), cfg_get(K_GUS_DMA), pv_windows());
    if (MessageBox(dialog, text, "Install Gravis UltraSound", MB_OKCANCEL | MB_ICONQUESTION) != IDOK) return;
    if (!pv_backup()) { pv_report(dialog); return; }
    if (!ExitWindowsExec(installer, "")) explain(dialog, "Windows could not launch the installer, or shutdown was cancelled. Exit Windows and run WINDOWS\\PIDOSAUD\\GUS\\PVRUN.EXE from DOS.");
}

BOOL FAR PASCAL __export AudioDialog(HWND dialog, UINT message, WPARAM wparam, LPARAM lparam)
{
    unsigned i, selected = 0;
    int card;
    (void)lparam;
    if (message == WM_INITDIALOG) {
        card = cfg_load("C:\\SYSTEM\\PIDOS.CFG") ? cfg_card() : -2;
        if (card == CARD_NONE) selected = AUDIO_PROFILE_COUNT - 1;
        for (i = 0; i < AUDIO_PROFILE_COUNT; ++i) {
            SendDlgItemMessage(dialog, IDC_AUDIO_CARD, CB_ADDSTRING, 0, (LPARAM)(LPCSTR)audio_profiles[i].label);
            if (card == audio_profiles[i].card) selected = i;
        }
        SendDlgItemMessage(dialog, IDC_AUDIO_CARD, CB_ADDSTRING, 0, (LPARAM)(LPCSTR)"AdLib (Windows driver)");
        SendDlgItemMessage(dialog, IDC_AUDIO_CARD, CB_ADDSTRING, 0, (LPARAM)(LPCSTR)"Game Blaster / CMS (unavailable)");
        SendDlgItemMessage(dialog, IDC_AUDIO_CARD, CB_ADDSTRING, 0, (LPARAM)(LPCSTR)"Tandy (unavailable)");
        if (card == CARD_ADLIB) selected = AUDIO_PROFILE_COUNT;
        if (card == CARD_CMS) selected = AUDIO_PROFILE_COUNT + 1;
        if (card == CARD_TANDY) selected = AUDIO_PROFILE_COUNT + 2;
        SendDlgItemMessage(dialog, IDC_AUDIO_CARD, CB_SETCURSEL, selected, 0);
        audio_info(dialog); return TRUE;
    }
    if (message == WM_COMMAND) switch (wparam) {
    case IDCANCEL: EndDialog(dialog, 0); return TRUE;
    case IDC_OPTIONS: pv_options(dialog); audio_info(dialog); return TRUE;
    case IDC_CONFIG_AUDIO: driver_configure(dialog, "wave"); return TRUE;
    case IDC_MIDI_MAP: driver_configure(dialog, "midimapper"); return TRUE;
    case IDC_WINDOWS_DRIVERS: if (pv_backup()) windows_drivers(dialog); else pv_report(dialog); return TRUE;
    case IDC_INSTALL_AUDIO: install_selected(dialog); return TRUE;
    }
    return FALSE;
}

void pv_audio(HWND owner)
{
    DialogBox(pv_instance(), MAKEINTRESOURCE(IDD_AUDIO), owner, AudioDialog);
}
