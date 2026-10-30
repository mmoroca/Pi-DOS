#ifndef PI_DOS_MIDI_PROFILES_H
#define PI_DOS_MIDI_PROFILES_H
#define MIDI_PROFILE_COUNT 6
#define MIDI_ASSET_ROOT "/dosdata/SYSTEM/MIDI"
enum { MIDI_NONE, MIDI_SC55, MIDI_ROLAND_GM, MIDI_MT32, MIDI_CM32L, MIDI_EXTERNAL };
#define DEFAULT_MIDI_PROFILE MIDI_SC55
typedef struct { const char *id, *label, *device, *asset, *model; } MidiProfile;
extern const MidiProfile midi_profiles[MIDI_PROFILE_COUNT];
int midi_profile_find(const char *id);
#endif
