#ifndef PI_DOS_AUDIO_INI_H
#define PI_DOS_AUDIO_INI_H
/* Replace only legacy sound VxD lines; preserve repeated unrelated devices. */
int pa_sound_vxd(const char *input, const char *output, const char *device);
#endif
