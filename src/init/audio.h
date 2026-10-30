#ifndef PI_DOS_AUDIO_H
#define PI_DOS_AUDIO_H

#include <stdbool.h>
#include <stddef.h>

bool pi_dos_audio_find(const char *output, const char *sound_root,
                       const char *asound_root, char *device, size_t size);
void pi_dos_audio_prepare(const char *config);

#endif
