/* Small, mouse-optional 80x25 DOS setup UI; no extender or FPU required. */
#include <dos.h>
#include <conio.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "config.h"

#define DEFAULT_CONFIG "C:\\SYSTEM\\PIDOS.CFG"
#define NORMAL 0x17
#define PANEL 0x1f
#define DIM 0x1b
#define SELECTED 0x30
#define BUTTON 0x70
#define FOCUSED 0x3f

static unsigned short saved_screen[2000];
static unsigned short __far *video;
static int mouse_present;
static unsigned char old_mode, old_page;
static unsigned short old_cursor, old_shape;

static int equal(const char *a, const char *b)
{
    return stricmp(a, b) == 0;
}

static void mouse_call(unsigned int function, union REGS *r)
{
    r->x.ax = function;
    int86(0x33, r, r);
}

static void mouse_visible(int visible)
{
    union REGS r;
    if (mouse_present) mouse_call(visible ? 1 : 2, &r);
}

static void screen_start(void)
{
    union REGS r;
    unsigned short __far *original;
    unsigned char __far *mouse_handler;
    int i;
    r.h.ah = 0x0f;
    int86(0x10, &r, &r);
    old_mode = r.h.al & 0x7f;
    old_page = r.h.bh;
    r.h.ah = 3;
    r.h.bh = old_page;
    int86(0x10, &r, &r);
    old_cursor = r.x.dx;
    old_shape = r.x.cx;
    original = (unsigned short __far *)MK_FP(old_mode == 7 ? 0xb000 : 0xb800,
                                            old_page * 4096U);
    for (i = 0; i < 2000; ++i) saved_screen[i] = original[i];
    r.x.ax = 3;
    int86(0x10, &r, &r);
    video = (unsigned short __far *)MK_FP(0xb800, 0);
    r.h.ah = 1;
    r.x.cx = 0x2000;
    int86(0x10, &r, &r);
    mouse_handler = (unsigned char __far *)_dos_getvect(0x33);
    mouse_present = 0;
    if ((FP_SEG(mouse_handler) || FP_OFF(mouse_handler)) && *mouse_handler != 0xcf) {
        mouse_call(0, &r);
        mouse_present = r.x.ax == 0xffff;
    }
    if (mouse_present) {
        /* Text-mode coordinates are eight mouse units per character cell. */
        r.x.cx = 0; r.x.dx = 639; mouse_call(7, &r);
        r.x.cx = 0; r.x.dx = 199; mouse_call(8, &r);
        r.x.bx = 0; r.x.cx = 0x77ff; r.x.dx = 0x7700; mouse_call(10, &r);
        mouse_visible(1);
    }
}

static void screen_end(void)
{
    union REGS r;
    unsigned short __far *original;
    int i;
    mouse_visible(0);
    r.x.ax = old_mode;
    int86(0x10, &r, &r);
    original = (unsigned short __far *)MK_FP(old_mode == 7 ? 0xb000 : 0xb800,
                                            old_page * 4096U);
    if (old_mode == 3 || old_mode == 7)
        for (i = 0; i < 2000; ++i) original[i] = saved_screen[i];
    r.h.ah = 5; r.h.al = old_page;
    int86(0x10, &r, &r);
    r.h.ah = 1; r.x.cx = old_shape;
    int86(0x10, &r, &r);
    r.h.ah = 2; r.h.bh = old_page; r.x.dx = old_cursor;
    int86(0x10, &r, &r);
}

static void fill(int x, int y, int width, int height, unsigned char color)
{
    int row, col;
    for (row = y; row < y + height; ++row)
        for (col = x; col < x + width; ++col)
            video[row * 80 + col] = ((unsigned int)color << 8) | ' ';
}

static void text(int x, int y, const char *s, unsigned char color, int width)
{
    int n;
    for (n = 0; n < width && *s; ++n)
        video[y * 80 + x + n] = ((unsigned int)color << 8) | (unsigned char)*s++;
}

static void box(int x, int y, int width, int height)
{
    int i;
    fill(x, y, width, height, NORMAL);
    for (i = 1; i < width - 1; ++i) {
        video[y*80 + x+i] = (PANEL << 8) | 196;
        video[(y+height-1)*80 + x+i] = (PANEL << 8) | 196;
    }
    for (i = 1; i < height - 1; ++i) {
        video[(y+i)*80 + x] = (PANEL << 8) | 179;
        video[(y+i)*80 + x+width-1] = (PANEL << 8) | 179;
    }
    video[y*80+x] = (PANEL << 8) | 218;
    video[y*80+x+width-1] = (PANEL << 8) | 191;
    video[(y+height-1)*80+x] = (PANEL << 8) | 192;
    video[(y+height-1)*80+x+width-1] = (PANEL << 8) | 217;
}

static void draw(int selected, int current, int focus, const char *path)
{
    int i;
    char line[160];
    const CpuProfile *p = selected >= 0 ? &cpu_profiles[selected] : NULL;
    mouse_visible(0);
    fill(0, 0, 80, 25, NORMAL);
    fill(0, 0, 80, 1, SELECTED);
    text(3, 0, "Pi-DOS Setup", SELECTED, 30);
    text(57, 0, "CPU configuration", SELECTED, 21);
    text(4, 2, "Current:", PANEL, 9);
    text(13, 2, current < 0 ? "Custom settings" : cpu_profiles[current].label, PANEL, 61);
    box(3, 4, 74, 12);
    text(6, 4, " CPU profile ", PANEL, 30);
    for (i = 0; i < PROFILE_COUNT; ++i) {
        unsigned char color = i == selected ? (focus == 0 ? SELECTED : BUTTON) : NORMAL;
        fill(5, 5+i, 70, 1, color);
        text(7, 5+i, i == selected ? "(*)" : "( )", color, 3);
        text(12, 5+i, cpu_profiles[i].label, color, 50);
    }
    if (p) {
        text(4, 16, p->description, PANEL, 72);
        sprintf(line, "CPU model: %s   Cycles: %s", p->type, p->cycles);
    } else {
        text(4, 16, "Current settings do not match a profile. Select one to replace them.", PANEL, 72);
        sprintf(line, "CPU model: %.22s   Cycles: %.22s", cfg_type, cfg_cycles);
    }
    text(4, 17, line, DIM, 72);
    text(4, 19, "MHz values are approximate performance equivalents.", DIM, 72);
    text(4, 20, "Save changes to PIDOS.CFG, then reboot Pi-DOS to apply.", PANEL, 72);
    fill(4, 22, 28, 1, focus == 1 ? FOCUSED : BUTTON);
    text(4, 22, "   Restore defaults (F3)    ", focus == 1 ? FOCUSED : BUTTON, 28);
    fill(44, 22, 14, 1, focus == 2 ? FOCUSED : BUTTON);
    text(44, 22, "  Save (F2)   ", focus == 2 ? FOCUSED : BUTTON, 14);
    fill(60, 22, 15, 1, focus == 3 ? FOCUSED : BUTTON);
    text(60, 22, " Cancel (Esc)  ", focus == 3 ? FOCUSED : BUTTON, 15);
    text(4, 23, mouse_present ? "Keyboard + mouse" : "Keyboard (no mouse driver)", DIM, 36);
    fill(0, 24, 80, 1, SELECTED);
    text(1, 24, "Arrows: select  Tab: focus  Enter: activate", SELECTED, 45);
    text(47, 24, path, SELECTED, 32);
    mouse_visible(1);
}

static unsigned int key_read(int blocking)
{
    union REGS r;
    if (!blocking && !kbhit()) return 0;
    r.h.ah = 0;
    int86(0x16, &r, &r);
    return r.x.ax;
}

static void message(const char *title, const char *body)
{
    union REGS r;
    unsigned int buttons = 0;
    unsigned int split = (unsigned int)strlen(body);
    char first[59];
    mouse_visible(0);
    fill(9, 9, 64, 7, 0x00);
    box(8, 8, 64, 7);
    text(11, 9, title, PANEL, 58);
    if (split > 58) {
        split = 58;
        while (split && body[split] != ' ') --split;
        if (!split) split = 58;
    }
    memcpy(first, body, split);
    first[split] = 0;
    text(11, 11, first, PANEL, 58);
    while (body[split] == ' ') ++split;
    text(11, 12, body + split, PANEL, 58);
    text(11, 13, "Press any key or click to continue.", DIM, 58);
    mouse_visible(1);
    /* Require a new click, rather than the Save button's held mouse button. */
    if (mouse_present) {
        do { mouse_call(3, &r); } while (r.x.bx & 3);
    }
    while (!key_read(0)) {
        if (mouse_present) {
            mouse_call(3, &r);
            buttons = r.x.bx & 3;
        }
        if (buttons) break;
        /* BIOS idle interrupt; avoid a hot busy loop when no input arrives. */
        int86(0x28, &r, &r);
    }
}

static int setup(const char *path)
{
    int current = cfg_profile(), selected = current, focus = 0, redraw = 1;
    int action, x, y, previous = 0;
    unsigned int key, scan, ch;
    union REGS r;
    screen_start();
    for (;;) {
        if (redraw) { draw(selected, current, focus, path); redraw = 0; }
        action = 0;
        key = key_read(0);
        scan = key >> 8;
        ch = key & 255;
        if (key) {
            redraw = 1;
            if (ch == 27 || (ch == 0 && (scan == 45 || scan == 46))) action = 2;
            else if (scan == 60 || (ch == 0 && scan == 31)) action = 1;
            else if (scan == 61 || (ch == 0 && scan == 19)) action = 3;
            else if (ch == 9) focus = (focus + 1) % 4;
            else if (scan == 15 && ch == 0) focus = (focus + 3) % 4;
            else if (scan == 72 || scan == 80 || scan == 71 || scan == 79) {
                focus = 0;
                if (scan == 71) selected = 0;
                else if (scan == 79) selected = PROFILE_COUNT - 1;
                else if (scan == 72) selected = selected <= 0 ? PROFILE_COUNT - 1 : selected - 1;
                else selected = (selected + 1) % PROFILE_COUNT;
            } else if (ch == 13 || (ch == ' ' && focus != 0)) {
                if (focus == 0 && selected < 0) selected = 0;
                else action = focus == 1 ? 3 : focus == 3 ? 2 : 1;
            } else if (ch == ' ' && focus == 0 && selected < 0) selected = 0;
        }
        if (mouse_present) {
            mouse_call(3, &r);
            x = r.x.cx / 8; y = r.x.dx / 8;
            if ((r.x.bx & 1) && !(previous & 1)) {
                if (x >= 5 && x < 75 && y >= 5 && y < 5 + PROFILE_COUNT) {
                    selected = y - 5; focus = 0; redraw = 1;
                } else if (y == 22 && x >= 4 && x < 32) action = 3;
                else if (y == 22 && x >= 44 && x < 58) action = 1;
                else if (y == 22 && x >= 60 && x < 75) action = 2;
            }
            previous = r.x.bx;
        }
        if (action == 3) { selected = cpu_profile_default(); focus = 1; redraw = 1; }
        if (action == 2) { screen_end(); return 0; }
        if (action == 1) {
            if (cfg_save(path, selected)) {
                message("CPU profile saved", "Restart Pi-DOS to apply. Previous configuration saved as .BAK.");
                screen_end();
                puts("CPU profile saved. Run REBOOT to apply the new settings.");
                return 0;
            }
            message("Unable to save", cfg_error);
            redraw = 1;
        }
        if (!key) int86(0x28, &r, &r);
    }
}

static void usage(void)
{
    puts("Pi-DOS Setup - CPU profiles\n"
         "PIDOSCFG                         Open keyboard/mouse setup\n"
         "PIDOSCFG /LIST                   List profiles\n"
         "PIDOSCFG /PROFILE id             Save a profile without opening setup\n"
         "Optional: /CONFIG path           Use another configuration file\n"
         "Changes apply after reboot. MHz labels are approximate.\n"
         "The previous configuration is backed up with a .BAK extension.\n"
         "Built with Open Watcom; runtime source and license: C:\\DOCS\\WATCOM.TXT");
}

int main(int argc, char **argv)
{
    const char *path = DEFAULT_CONFIG, *profile = NULL;
    int i, chosen = -1, list = 0;
    for (i = 1; i < argc; ++i) {
        if (equal(argv[i], "/?") || equal(argv[i], "/HELP")) { usage(); return 0; }
        if (equal(argv[i], "/LIST")) list = 1;
        else if (equal(argv[i], "/PROFILE") && i+1 < argc) profile = argv[++i];
        else if (equal(argv[i], "/CONFIG") && i+1 < argc) path = argv[++i];
        else { usage(); return 1; }
    }
    if (list) {
        for (i = 0; i < PROFILE_COUNT; ++i)
            printf("%-12s %-29s %s\n", cpu_profiles[i].id,
                   cpu_profiles[i].label, cpu_profiles[i].cycles);
        return 0;
    }
    if (profile) {
        for (i = 0; i < PROFILE_COUNT; ++i)
            if (equal(profile, cpu_profiles[i].id)) chosen = i;
        if (chosen < 0) { puts("Unknown profile. Run PIDOSCFG /LIST."); return 1; }
    }
    if (!cfg_load(path)) { puts(cfg_error); return 1; }
    if (profile) {
        if (!cfg_save(path, chosen)) { puts(cfg_error); return 1; }
        printf("Saved %s. Restart Pi-DOS to apply.\n", cpu_profiles[chosen].label);
        return 0;
    }
    return setup(path);
}
