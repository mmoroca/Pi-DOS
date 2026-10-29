#ifndef PI_DOS_INPUT_OPTIONS_H
#define PI_DOS_INPUT_OPTIONS_H
#include <ctype.h>
#include <string.h>

/* Deliberately exclude F12 (the emulator's Host key). Shared C89 validation. */
#define PIDOS_SHORTCUT_CHOICES "none=None|Ctrl+Alt+Delete|Ctrl+Alt+F1|Ctrl+Alt+F2|Ctrl+Alt+F3|Ctrl+Alt+F4|Ctrl+Alt+F5|Ctrl+Alt+F6|Ctrl+Alt+F7|Ctrl+Alt+F8|Ctrl+Alt+F9|Ctrl+Alt+F10"

#ifdef PIDOS_INPUT_OPTIONS_VALIDATE
/* 0=disabled, 1=Delete (including keypad Delete), 2..11=F1..F10. */
static int pidos_shortcut_key(const char *value)
{
    char word[24];
    unsigned int i;
    if (!value || strlen(value) >= sizeof(word)) return -1;
    for (i=0; value[i]; ++i) word[i]=(char)tolower((unsigned char)value[i]);
    word[i]=0;
    if (!strcmp(word,"none")) return 0;
    if (!strcmp(word,"ctrl+alt+delete")) return 1;
    if (!strncmp(word,"ctrl+alt+f",10)) {
        if (word[10]>='1' && word[10]<='9' && !word[11]) return word[10]-'0'+1;
        if (!strcmp(word+10,"10")) return 11;
    }
    return -1;
}
#endif
#endif
