#ifndef PI_DOS_LCD_VIDEO_H
#define PI_DOS_LCD_VIDEO_H

/* Shared by the SDL2 surface patch and the host geometry check. */
typedef struct { int x, y, w, h; } PidosViewport;
static inline PidosViewport pidos_lcd_fit(int width, int height, int source_width,
                                         int source_height, int aspect_4_3)
{
    PidosViewport result = {0, 0, width, height};
    long long rw = aspect_4_3 ? 4 : source_width;
    long long rh = aspect_4_3 ? 3 : source_height;
    if (width <= 0 || height <= 0 || rw <= 0 || rh <= 0) {
        result.w = result.h = 0;
        return result;
    }
    if ((long long)width * rh > (long long)height * rw)
        result.w = (int)((long long)height * rw / rh);
    else
        result.h = (int)((long long)width * rh / rw);
    if (result.w < 1) result.w = 1;
    if (result.h < 1) result.h = 1;
    result.x = (width - result.w) / 2;
    result.y = (height - result.h) / 2;
    return result;
}
#endif
