#define _GNU_SOURCE
#include "midi.h"
#include "config.h"
#include "midi_profiles.h"
#include "midi_ports.h"
#include <sys/stat.h>
#include <unistd.h>

void pi_dos_midi_label(const char *config, char *label, unsigned int size) {
    char profile[40], enabled[32] = "none";
    if (pi_dos_config_value(config, "midi", "mpu401", enabled, sizeof(enabled)) && !strcasecmp(enabled, "none")) {
        snprintf(label, size, "Disabled"); return;
    }
    const char *status = getenv("PI_DOS_MIDI_STATUS");
    if (status) { snprintf(label, size, "%s", status); return; }
    int selected = pi_dos_config_value(config, "pidos", "midi_profile", profile, sizeof(profile)) ? midi_profile_find(profile) : -1;
    snprintf(label, size, "%s", selected >= 0 ? midi_profiles[selected].label : !strcasecmp(enabled, "none") ? "Disabled" : "Custom MIDI");
}
static bool asset(const char *root, const char *relative, bool soundfont) {
    char path[PATH_MAX]; struct stat status;
    if (snprintf(path, sizeof(path), "%s/%s", root, relative) >= (int)sizeof(path) ||
        stat(path, &status) || !S_ISREG(status.st_mode) || access(path, R_OK)) return false;
    if (!soundfont) return status.st_size == 65536 || status.st_size == 524288 || status.st_size == 1048576;
    FILE *f = fopen(path, "rb"); char header[12];
    if (!f) return false;
    bool valid = fread(header, 1, sizeof(header), f) == sizeof(header) && !memcmp(header, "RIFF", 4) && !memcmp(header + 8, "sfbk", 4);
    fclose(f); return valid;
}
bool pi_dos_midi_prepare(const char *source, const char *runtime, const char *root) {
    char value[80];
    unsetenv("PI_DOS_MIDI_STATUS"); unsetenv("PI_DOS_MIDI_MANAGED");
    if (!pi_dos_config_value(source, "pidos", "midi_profile", value, sizeof(value))) return true;
    int profile = midi_profile_find(value);
    const MidiProfile *p = &midi_profiles[profile >= 0 ? profile : MIDI_NONE];
    char mode[32] = "intelligent", port[23] = "", config[PATH_MAX] = "", romdir[PATH_MAX] = "";
    bool available = profile >= 0;
    const char *status = profile >= 0 ? p->label : "Invalid MIDI profile";
    pi_dos_config_value(source, "midi", "mpu401", mode, sizeof(mode));
    if (profile == MIDI_SC55 || profile == MIDI_ROLAND_GM) {
        available = asset(root, p->asset, true);
        snprintf(config, sizeof(config), "%s/%s", root, p->asset);
        if (!available) status = "Missing SoundFont";
    } else if (profile == MIDI_MT32 || profile == MIDI_CM32L) {
        char control[80], pcm[80]; const char *model = profile == MIDI_MT32 ? "MT32" : "CM32L";
        snprintf(control, sizeof(control), "%s/%s_CONTROL.ROM", p->asset, model);
        snprintf(pcm, sizeof(pcm), "%s/%s_PCM.ROM", p->asset, model);
        available = asset(root, control, false) && asset(root, pcm, false);
        snprintf(romdir, sizeof(romdir), "%s/%s", root, p->asset);
        if (!available) status = "Missing MIDI ROMs";
    } else if (profile == MIDI_EXTERNAL) {
        int client, endpoint;
        available = pi_dos_config_value(source, "pidos", "midi_port", port, sizeof(port)) &&
                    !strncmp(port, "pidos:", 6) && strlen(port) == 22 && pidos_midi_resolve(port, &client, &endpoint);
        snprintf(config, sizeof(config), "%s%s", port,
                 pi_dos_config_value(source, "pidos", "midi_delaysysex", value, sizeof(value)) && !strcasecmp(value, "true") ? " delaysysex" : "");
        if (!available) status = "USB MIDI unavailable";
    }
    if (profile == MIDI_NONE || !strcasecmp(mode, "none")) { profile = MIDI_NONE; status = "Disabled"; }
    bool active = available && profile != MIDI_NONE;
    FILE *out = fopen(runtime, "a");
    if (!out) return false;
    fprintf(out, "\n[midi]\nmpu401=%s\nmididevice=%s\nmidiconfig=%s\nmt32.model=%s\nmt32.romdir=%s\n",
            active ? !strcasecmp(mode, "uart") ? "uart" : "intelligent" : "none",
            active ? p->device : "none", config, p->model, romdir);
    bool ok = !ferror(out); if (fclose(out)) ok = false;
    if (!ok) return false;
    setenv("PI_DOS_MIDI_MANAGED", "1", 1); setenv("PI_DOS_MIDI_STATUS", status, 1);
    fprintf(stderr, "Pi-DOS: MIDI %s\n", status);
    return true;
}
