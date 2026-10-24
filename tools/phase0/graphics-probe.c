/* Diagnostic only: GDB calls this at KMS swap entry, after renderer drawing.
 * It reads the current GPU back buffer; it does not replace the renderer. */
#include <SDL.h>
#include <GLES2/gl2.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void json_string(FILE *f, const char *s)
{
    fputc('"', f);
    for (; s && *s; ++s) {
        unsigned char c = (unsigned char)*s;
        if (c == '"' || c == '\\') fputc('\\', f);
        if (c < 32) fprintf(f, "\\u%04x", c);
        else fputc(c, f);
    }
    fputc('"', f);
}

int armdos_capture_frame(void *window)
{
    const char *directory = getenv("ARMDOS_CAPTURE_DIR");
    SDL_Renderer *renderer = SDL_GetRenderer((SDL_Window *)window);
    SDL_RendererInfo info;
    GLint viewport[4], alignment, row_length = 0, skip_rows = 0, skip_pixels = 0;
    int profile = 0;
    char path[4096];
    if (!directory || !renderer || SDL_GetRendererInfo(renderer, &info)) return 1;
    const char *version = (const char *)glGetString(GL_VERSION);
    const char *gpu = (const char *)glGetString(GL_RENDERER);
    const char *vendor = (const char *)glGetString(GL_VENDOR);
    if (!version || !gpu || !vendor) return 2;
    glGetIntegerv(GL_VIEWPORT, viewport);
    int w = viewport[2], h = viewport[3];
    if (w < 1 || h < 1 || w > 8192 || h > 8192) return 3;
    unsigned char *pixels = malloc((size_t)w * h * 4);
    unsigned char *row = malloc((size_t)w * 3);
    if (!pixels || !row) { free(pixels); free(row); return 4; }
    for (int i = 0; i < 8 && glGetError() != GL_NO_ERROR; ++i) { }
    /* Desktop GL and ES3 have pack row/skip state in addition to alignment. */
    int extended_pack = strncmp(version, "OpenGL ES 2.", 12) != 0;
    if (extended_pack) {
        glGetIntegerv(0x0d02, &row_length);
        glGetIntegerv(0x0d03, &skip_rows);
        glGetIntegerv(0x0d04, &skip_pixels);
        glPixelStorei(0x0d02, 0);
        glPixelStorei(0x0d03, 0);
        glPixelStorei(0x0d04, 0);
    }
    glGetIntegerv(GL_PACK_ALIGNMENT, &alignment);
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glReadPixels(viewport[0], viewport[1], w, h, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
    GLenum error = glGetError();
    glPixelStorei(GL_PACK_ALIGNMENT, alignment);
    if (extended_pack) {
        glPixelStorei(0x0d02, row_length);
        glPixelStorei(0x0d03, skip_rows);
        glPixelStorei(0x0d04, skip_pixels);
    }
    if (error != GL_NO_ERROR) { free(pixels); free(row); return 5; }
    snprintf(path, sizeof(path), "%s/frame.ppm", directory);
    FILE *f = fopen(path, "wb");
    if (!f) { free(pixels); free(row); return 6; }
    fprintf(f, "P6\n%d %d\n255\n", w, h);
    int failed = 0;
    for (int y = h - 1; y >= 0; --y) {
        for (int x = 0; x < w; ++x)
            for (int c = 0; c < 3; ++c)
                row[x * 3 + c] = pixels[((size_t)y * w + x) * 4 + c];
        if (fwrite(row, 3, (size_t)w, f) != (size_t)w) failed = 1;
    }
    if (fclose(f)) failed = 1;
    free(pixels);
    free(row);
    if (failed) return 7;
    SDL_GL_GetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, &profile);
    snprintf(path, sizeof(path), "%s/renderer.json", directory);
    f = fopen(path, "w");
    if (!f) return 8;
    fprintf(f, "{\n  \"sdl_renderer\": "); json_string(f, info.name);
    fprintf(f, ",\n  \"renderer_flags\": %u,\n  \"gl_profile_mask\": %d,\n  \"gl_version\": ", info.flags, profile);
    json_string(f, version);
    fprintf(f, ",\n  \"gl_renderer\": "); json_string(f, gpu);
    fprintf(f, ",\n  \"gl_vendor\": "); json_string(f, vendor);
    fprintf(f, ",\n  \"width\": %d,\n  \"height\": %d\n}\n", w, h);
    if (fclose(f)) return 9;
    snprintf(path, sizeof(path), "%s/CAPTURE.OK", directory);
    f = fopen(path, "w");
    if (!f) return 10;
    return fclose(f) ? 11 : 0;
}
