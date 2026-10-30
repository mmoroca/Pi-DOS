#include <ctype.h>
#include "video_profiles.h"
const VideoProfile video_profiles[VIDEO_PROFILE_COUNT] = {
 {"cga", "IBM CGA"}, {"hercules", "Hercules"}, {"tandy", "Tandy"},
 {"ega", "IBM EGA"}, {"vgaonly", "IBM VGA"},
 {"svga_et4000", "Tseng Labs ET4000"}, {"svga_s3", "S3 Trio64"},
 {"svga_dosbox", "VESA VBE 3.0"}, {"svga_dosbox_vbe2", "VESA VBE 2.0"}
};
static int same(const char *a,const char *b) {
 while(*a && *b) if(tolower((unsigned char)*a++)!=tolower((unsigned char)*b++)) return 0;
 return *a==*b;
}
int video_profile_find(const char *machine) {
 int i;
 if(!machine) return -1;
 if(same(machine,"svga_s3trio64")) machine="svga_s3";
 for(i=0;i<VIDEO_PROFILE_COUNT;++i) if(same(machine,video_profiles[i].id)) return i;
 return -1;
}
/* General display resolutions advertised by dac-crt.bin; DOS timings stay CRT-only. */
const char *lcd_modes[LCD_MODE_COUNT] = {
 "640x480", "800x600", "832x624", "1024x768", "1280x720", "1920x1080"
};
int lcd_mode_find(const char *mode) {
 int i;
 if(!mode) return -1;
 for(i=0;i<LCD_MODE_COUNT;++i) if(same(mode,lcd_modes[i])) return i;
 return -1;
}
