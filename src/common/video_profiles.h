#ifndef PI_DOS_VIDEO_PROFILES_H
#define PI_DOS_VIDEO_PROFILES_H
#define VIDEO_PROFILE_COUNT 9
#define DEFAULT_VIDEO_PROFILE 6
typedef struct { const char *id, *label; } VideoProfile;
extern const VideoProfile video_profiles[VIDEO_PROFILE_COUNT];
int video_profile_find(const char *machine);
#define LCD_MODE_COUNT 6
#define DEFAULT_LCD_MODE 3
extern const char *lcd_modes[LCD_MODE_COUNT];
int lcd_mode_find(const char *mode);
#endif
