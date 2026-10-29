#ifndef PI_DOS_CONTROLLERS_H
#define PI_DOS_CONTROLLERS_H
/* SDL2 appliance input. DOS hardware conversion remains in DOSBox-X. */
#include <SDL.h>
#include <stdlib.h>
#include <string.h>

static bool pidos_native_controllers()
{
    const char *value=getenv("PI_DOS_SUPERVISED");
    return value && !strcmp(value,"1");
}

struct PidosControllerState {
    bool connected = false;
    Sint16 axes[4] = {};
    bool buttons[6] = {};
    Uint8 hat = 0;
};

class PidosControllers {
    struct Slot {
        SDL_Joystick *joystick = nullptr;
        SDL_GameController *controller = nullptr;
    } slots[2];
    bool initialized = false;
    bool owns_subsystem = false;
    Uint32 next_scan = 0;
    bool rescan = true;
    static bool option(const char *name, const char *value) {
        const char *text=getenv(name);
        return text && !strcmp(text,value);
    }
    void close(Slot &slot) {
        if (slot.controller) SDL_GameControllerClose(slot.controller);
        else if (slot.joystick) SDL_JoystickClose(slot.joystick);
        slot.controller=nullptr; slot.joystick=nullptr;
    }
public:
    ~PidosControllers() { reset(); }
    void reset() {
        for (auto &slot : slots) close(slot);
        if (owns_subsystem) SDL_QuitSubSystem(SDL_INIT_GAMECONTROLLER);
        owns_subsystem=false; initialized=false; rescan=true;
    }
    void changed() { rescan=true; }
    void update() {
        if (!initialized) {
            /* Linux kernel drivers supply input; SDL mappings normalize known pads. */
            if (!(SDL_WasInit(SDL_INIT_GAMECONTROLLER) & SDL_INIT_GAMECONTROLLER))
                owns_subsystem=SDL_InitSubSystem(SDL_INIT_GAMECONTROLLER)==0;
            initialized=true; rescan=true;
        }
        for (auto &slot : slots) if (slot.joystick && !SDL_JoystickGetAttached(slot.joystick)) {
            close(slot); rescan=true;
        }
        Uint32 now=SDL_GetTicks();
        if (!rescan && !SDL_TICKS_PASSED(now,next_scan)) return;
        rescan=false; next_scan=now+250;
        for (int index=0; index<SDL_NumJoysticks(); ++index) {
            SDL_JoystickID id=SDL_JoystickGetDeviceInstanceID(index);
            if (id<0) continue;
            bool assigned=false;
            for (const auto &slot : slots)
                if (slot.joystick && SDL_JoystickInstanceID(slot.joystick)==id) assigned=true;
            if (assigned) continue;
            for (auto &slot : slots) if (!slot.joystick) {
                if (!option("PI_DOS_CONTROLLER_LAYOUT","raw") && SDL_IsGameController(index))
                    slot.controller=SDL_GameControllerOpen(index);
                slot.joystick=slot.controller ? SDL_GameControllerGetJoystick(slot.controller) : SDL_JoystickOpen(index);
                if (slot.joystick) break;
            }
        }
    }
    PidosControllerState sample(unsigned int number) {
        update();
        PidosControllerState state;
        if (number>=2 || !slots[number].joystick) return state;
        state.connected=true;
        const auto &slot=slots[number];
        if (slot.controller) {
            const SDL_GameControllerAxis axes[]={SDL_CONTROLLER_AXIS_LEFTX,SDL_CONTROLLER_AXIS_LEFTY,
                SDL_CONTROLLER_AXIS_RIGHTX,SDL_CONTROLLER_AXIS_RIGHTY};
            const SDL_GameControllerButton buttons[]={SDL_CONTROLLER_BUTTON_A,SDL_CONTROLLER_BUTTON_B,
                SDL_CONTROLLER_BUTTON_X,SDL_CONTROLLER_BUTTON_Y,SDL_CONTROLLER_BUTTON_LEFTSHOULDER,SDL_CONTROLLER_BUTTON_RIGHTSHOULDER};
            for (int i=0;i<4;++i) state.axes[i]=SDL_GameControllerGetAxis(slot.controller,axes[i]);
            for (int i=0;i<6;++i) state.buttons[i]=SDL_GameControllerGetButton(slot.controller,buttons[i])!=0;
            if (SDL_GameControllerGetButton(slot.controller,SDL_CONTROLLER_BUTTON_DPAD_UP)) state.hat|=SDL_HAT_UP;
            if (SDL_GameControllerGetButton(slot.controller,SDL_CONTROLLER_BUTTON_DPAD_DOWN)) state.hat|=SDL_HAT_DOWN;
            if (SDL_GameControllerGetButton(slot.controller,SDL_CONTROLLER_BUTTON_DPAD_LEFT)) state.hat|=SDL_HAT_LEFT;
            if (SDL_GameControllerGetButton(slot.controller,SDL_CONTROLLER_BUTTON_DPAD_RIGHT)) state.hat|=SDL_HAT_RIGHT;
        } else {
            for (int i=0;i<4 && i<SDL_JoystickNumAxes(slot.joystick);++i) state.axes[i]=SDL_JoystickGetAxis(slot.joystick,i);
            for (int i=0;i<6 && i<SDL_JoystickNumButtons(slot.joystick);++i) state.buttons[i]=SDL_JoystickGetButton(slot.joystick,i)!=0;
            if (SDL_JoystickNumHats(slot.joystick)>0) state.hat=SDL_JoystickGetHat(slot.joystick,0);
        }
        if (option("PI_DOS_CONTROLLER_DIRECTION","dpad")) state.axes[0]=state.axes[1]=0;
        if (!option("PI_DOS_CONTROLLER_DIRECTION","stick")) {
            if (state.hat & (SDL_HAT_LEFT|SDL_HAT_RIGHT)) state.axes[0]=(state.hat&SDL_HAT_LEFT)?-32767:32767;
            if (state.hat & (SDL_HAT_UP|SDL_HAT_DOWN)) state.axes[1]=(state.hat&SDL_HAT_UP)?-32767:32767;
        }
        if (option("PI_DOS_CONTROLLER_SWAP_BUTTONS","true")) {
            bool first=state.buttons[0]; state.buttons[0]=state.buttons[1]; state.buttons[1]=first;
        }
        return state;
    }
};
static PidosControllers pidos_controllers;
#endif
