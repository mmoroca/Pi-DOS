#ifndef PI_DOS_VIDEO_PROFILES_H
#define PI_DOS_VIDEO_PROFILES_H
#define VIDEO_PROFILE_COUNT 9
#define DEFAULT_VIDEO_PROFILE 6
typedef struct { const char *id, *label; } VideoProfile;
extern const VideoProfile video_profiles[VIDEO_PROFILE_COUNT];
int video_profile_find(const char *machine);
#endif
