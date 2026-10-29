#ifndef PI_DOS_REBOOT_SHORTCUT_H
#define PI_DOS_REBOOT_SHORTCUT_H
/* Included after SDL.h. PID 1 exports validated shortcut codes before exec. */
#if defined(LINUX) && defined(C_SDL2)
#include <errno.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
void PauseDOSBox(bool pressed);
static bool pi_dos_reboot_requested = false;
static unsigned char pi_dos_shortcut_held[SDL_NUM_SCANCODES] = {};

static bool pi_dos_supervisor_stopping(void)
{
    const char *supervised = getenv("PI_DOS_SUPERVISED");
    return supervised && !strcmp(supervised, "1") && getppid() == 1 &&
           access("/run/pi-dos-stop", F_OK) == 0;
}
static int pi_dos_shortcut_code(const char *name, int fallback)
{
    const char *value=getenv(name);
    if (!value) return fallback;
    char *end=nullptr;
    long code=strtol(value,&end,10);
    return *value && !*end && code>=0 && code<=11 ? (int)code : fallback;
}
static bool pi_dos_reboot_shortcut(const SDL_Event &event, bool ctrl, bool alt)
{
    const char *supervised=getenv("PI_DOS_SUPERVISED");
    if (!supervised || strcmp(supervised,"1") || getppid()!=1) return false;
    if (event.type!=SDL_KEYDOWN && event.type!=SDL_KEYUP) return false;
    const SDL_Scancode key=event.key.keysym.scancode;
    if (key<=SDL_SCANCODE_UNKNOWN || key>=SDL_NUM_SCANCODES) return false;
    if (event.type==SDL_KEYUP && pi_dos_shortcut_held[key]) {
        int action=pi_dos_shortcut_held[key]; pi_dos_shortcut_held[key]=0;
        /* Run pause on release so its event-queue flush cannot swallow this release. */
        if (action==3) PauseDOSBox(true);
        return true;
    }
    if (event.type!=SDL_KEYDOWN) return false;
    if (pi_dos_shortcut_held[key]) return true;
    /* Shift/GUI variants stay available to the guest. Caps/NumLock do not matter. */
    if (!ctrl || !alt || (event.key.keysym.mod & (KMOD_SHIFT|KMOD_GUI))) return false;
    int code=(key==SDL_SCANCODE_DELETE || key==SDL_SCANCODE_KP_PERIOD)?1:
        (key>=SDL_SCANCODE_F1 && key<=SDL_SCANCODE_F10 ? key-SDL_SCANCODE_F1+2 : 0);
    if (!code) return false;
    const char *names[]={"PI_DOS_SHORTCUT_REBOOT","PI_DOS_SHORTCUT_POWEROFF","PI_DOS_SHORTCUT_PAUSE"};
    int action=0;
    for (int i=0;i<3;++i) if (code==pi_dos_shortcut_code(names[i],i==0?1:0)) { action=i+1; break; }
    if (!action) return false;
    pi_dos_shortcut_held[key]=(unsigned char)action;
    if (event.key.repeat || action==3 || pi_dos_reboot_requested) return true;
    /* PID 1 owns storage synchronization and emulator teardown for both actions. */
    if (kill(1,action==1?SIGINT:SIGTERM)==0) {
        pi_dos_reboot_requested=true;
        LOG_MSG("Pi-DOS: system shortcut requested supervised %s",action==1?"reboot":"poweroff");
    } else LOG_MSG("Pi-DOS: system shortcut failed: %s",strerror(errno));
    return true;
}
#endif
#endif
