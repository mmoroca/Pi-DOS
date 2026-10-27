#ifndef PI_DOS_REBOOT_SHORTCUT_H
#define PI_DOS_REBOOT_SHORTCUT_H
/* Included after SDL.h by the SDL2 runtime; PID 1 opts in before exec. */
#if defined(LINUX) && defined(C_SDL2)
#include <errno.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static bool pi_dos_reboot_requested = false;

static bool pi_dos_reboot_shortcut(const SDL_Event &event, bool ctrl, bool alt)
{
    const char *supervised = getenv("PI_DOS_SUPERVISED");
    if (!supervised || strcmp(supervised, "1") || getppid() != 1) return false;
    if (event.type != SDL_KEYDOWN && event.type != SDL_KEYUP) return false;
    const SDL_Scancode key = event.key.keysym.scancode;
    if (key != SDL_SCANCODE_DELETE && key != SDL_SCANCODE_KP_PERIOD) return false;
    if (pi_dos_reboot_requested) return true;
    if (event.type != SDL_KEYDOWN || !ctrl || !alt) return false;
    if (event.key.repeat) return true;

    /* Wait for PID 1's SIGTERM: exiting here could race its reboot request. */
    if (kill(1, SIGINT) == 0) {
        pi_dos_reboot_requested = true;
        LOG_MSG("Pi-DOS: Ctrl+Alt+Delete requested a supervised reboot");
    } else {
        LOG_MSG("Pi-DOS: reboot request failed: %s", strerror(errno));
    }
    return true; /* Never pass a recognized request on to the guest reset. */
}
#endif
#endif
