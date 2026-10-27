#define _POSIX_C_SOURCE 200809L
#include "video.h"
#include "config.h"
#include "video_profiles.h"
#include <stdio.h>
#include <stdlib.h>
#include <strings.h>

bool pi_dos_video_prepare(const char *source, const char *runtime)
{
    char monitor[32], mode[32], aspect[32];
    unsetenv("PI_DOS_LCD_MODE");
    unsetenv("PI_DOS_LCD_ASPECT");
    FILE *out = fopen(runtime, "w");
    if (!out) return false;
    /* A missing profile in an existing installation leaves advanced settings intact. */
    if (source && !pi_dos_config_value(source, "pidos", "monitor", monitor, sizeof(monitor)))
        return fclose(out) == 0;
    bool lcd = !source || strcasecmp(monitor, "crt");
    if (lcd) {
        int selected = pi_dos_config_value(source, "pidos", "lcd_mode", mode, sizeof(mode)) ? lcd_mode_find(mode) : -1;
        const char *resolution = lcd_modes[selected >= 0 ? selected : DEFAULT_LCD_MODE];
        bool original = pi_dos_config_value(source, "pidos", "lcd_aspect", aspect, sizeof(aspect)) &&
                        !strcasecmp(aspect, "original");
        /* Separate source pixels from the fixed host surface. Aspect fitting happens once,
         * during final presentation; the emulated VGA clock is unchanged. */
        fprintf(out, "[sdl]\nfullscreen=true\noutput=surface\nfullresolution=%s\n"
                     "[render]\nchar9=true\naspect=false\ndoublescan=false\nmodeswitch=false\nscaler=none\n",
                resolution);
        if (setenv("PI_DOS_LCD_MODE", resolution, 1) ||
            setenv("PI_DOS_LCD_ASPECT", original ? "original" : "4:3", 1)) {
            fclose(out); return false;
        }
    } else {
        fputs("[sdl]\nfullscreen=true\noutput=surface\nfullresolution=original\n"
              "[render]\nchar9=true\naspect=false\ndoublescan=true\nmodeswitch=true\nscaler=none\n", out);
    }
    bool valid = !ferror(out);
    if (fclose(out)) valid = false;
    return valid;
}
