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
 if(same(machine,"svga_s3trio64")) return DEFAULT_VIDEO_PROFILE;
 for(i=0;i<VIDEO_PROFILE_COUNT;++i) if(same(machine,video_profiles[i].id)) return i;
 return -1;
}
