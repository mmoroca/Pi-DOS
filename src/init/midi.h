#ifndef PI_DOS_MIDI_H
#define PI_DOS_MIDI_H
#include <stdbool.h>
bool pi_dos_midi_prepare(const char *source, const char *runtime, const char *asset_root);
void pi_dos_midi_label(const char *config, char *label, unsigned int size);
#endif
