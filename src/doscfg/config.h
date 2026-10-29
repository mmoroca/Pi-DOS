#ifndef PIDOSCFG_CONFIG_H
#define PIDOSCFG_CONFIG_H

#include "cpu_profiles.h"
#include "video_profiles.h"
#include "midi_profiles.h"
#define CFG_VALUE_SIZE 80
#define CFG_PATH_SIZE 128
enum { PAGE_CPU, PAGE_MEMORY, PAGE_VIDEO, PAGE_SOUND, PAGE_MIXER, PAGE_INPUT, PAGE_NETWORK, PAGE_SYSTEM, PAGE_COUNT };
enum {
    K_CPU_TYPE, K_CPU_CORE, K_CPU_CYCLES, K_RAM, K_RAM_KB, K_MACHINE,
    K_SB_TYPE, K_SB_PORT, K_SB_IRQ, K_SB_DMA, K_SB_HDMA, K_OPL, K_CMS,
    K_GUS, K_GUS_TYPE, K_GUS_PORT, K_GUS_IRQ, K_GUS_DMA, K_GUS_RAM, K_GUS_DIR,
    K_TANDY, K_SPEAKER,
    K_VOL_MASTER, K_VOL_SB, K_VOL_FM, K_VOL_GUS, K_VOL_CMS, K_VOL_SPEAKER,
    K_VOL_TANDY, K_VOL_TANDY_DAC, K_VOL_DISNEY, K_VOL_MIDI,
    K_KEYBOARD, K_COUNTRY, K_TIMEZONE,
    K_VOODOO, K_VOODOO_MEM, K_GLIDE, K_DIAGNOSTICS,
    K_MIDI_PROFILE, K_MPU, K_MPU_BASE, K_MPU_IRQ, K_MIDI_DEVICE, K_MIDI_CONFIG,
    K_MT32_MODEL, K_MT32_DIR, K_MIDI_PORT, K_MIDI_DELAY,
    K_MONITOR, K_LCD_MODE, K_LCD_ASPECT, K_LCD_LINEAR,
    K_JOY_TYPE, K_CONTROLLER_LAYOUT, K_CONTROLLER_DIRECTION, K_CONTROLLER_SWAP,
    K_JOY_DEADZONE, K_JOY_DEADZONE2, K_JOY_DEADZONE3, K_JOY_TIMED, K_JOY_AUTOFIRE,
    K_SHORTCUT_REBOOT, K_SHORTCUT_POWEROFF, K_SHORTCUT_PAUSE,
    K_SHORTCUT_SCREENSHOT, K_SHORTCUT_NEXT_CD, K_SHORTCUT_NEXT_FLOPPY,
    K_IPX, K_IPX_MODE, K_IPX_SERVER, K_IPX_PORT, K_NE2000, K_NET_BACKEND,
    K_NET_BASE, K_NET_IRQ, K_NET_MAC, K_NET_INTERFACE, K_NET_STARTUP,
    K_NET_IP, K_NET_MASK, K_NET_GATEWAY, K_NET_DNS, K_NET_TCP_PORTS, K_NET_UDP_PORTS,
    K_NET_IPX_PATH, K_NET_IPX_FRAME, K_NET_WINDOWS, K_COUNT
};
enum { CARD_NONE, CARD_TANDY, CARD_ADLIB, CARD_CMS, CARD_SB2, CARD_PRO2,
       CARD_SB16, CARD_GUS, CARD_COUNT };
typedef struct { const char *section, *key, *default_value; int page; } CfgSpec;
extern const CfgSpec cfg_specs[K_COUNT];
extern const char *card_names[CARD_COUNT];
extern char cfg_error[160], cfg_type[CFG_VALUE_SIZE], cfg_core[CFG_VALUE_SIZE], cfg_cycles[CFG_VALUE_SIZE];
const char *cfg_get(int key);
int cfg_set(int key, const char *value);
int cfg_changed(void);
int cfg_is_dirty(int key);
void cfg_seed(int key, const char *value);
int cfg_load(const char *path);
int cfg_profile(void);
int cfg_select_cpu(int profile);
int cfg_card(void);
void cfg_select_card(int card);
void cfg_toggle_voodoo(void);
int cfg_midi(void);
void cfg_select_midi(int profile);
void cfg_reset_page(int page);
int cfg_save_changes(const char *path);
int cfg_save(const char *path, int profile);

#endif
