#define _POSIX_C_SOURCE 200809L
#include "audio.h"
#include "config.h"
#include <errno.h>
#include <glob.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <time.h>
#include <unistd.h>

/* Select only playback PCMs. Card numbers can change when USB is attached;
 * pass the discovered ALSA ID to SDL instead. With multiple USB DACs, choose
 * the first ID alphabetically, then its lowest playback device number. */
bool pi_dos_audio_find(const char *output, const char *sound_root,
                       const char *asound_root, char *device, size_t size)
{
    char pattern[PATH_MAX], best[64] = "";
    unsigned int best_pcm = 0;
    int best_rank = 2;
    glob_t paths = {0};
    snprintf(pattern, sizeof(pattern), "%s/pcmC*D*p", sound_root);
    if (glob(pattern, 0, NULL, &paths)) { globfree(&paths); return false; }
    for (size_t i = 0; i < paths.gl_pathc; ++i) {
        unsigned int card, pcm;
        int end = 0, rank = 0;
        const char *name = strrchr(paths.gl_pathv[i], '/');
        if (!name || sscanf(name + 1, "pcmC%uD%up%n", &card, &pcm, &end) != 2 ||
            !end || name[1 + end]) continue;
        char path[PATH_MAX], id[64];
        snprintf(path, sizeof(path), "%s/card%u/id", sound_root, card);
        FILE *input = fopen(path, "r");
        if (!input) continue;
        bool valid = fgets(id, sizeof(id), input) != NULL;
        fclose(input);
        if (!valid) continue;
        id[strcspn(id, "\r\n")] = 0;
        if (!*id) continue;
        snprintf(path, sizeof(path), "%s/card%u/usbbus", asound_root, card);
        bool usb = !access(path, F_OK);
        if (!strcasecmp(output, "hdmi")) {
            if (usb || pcm || (strcmp(id, "vc4hdmi0") && strcmp(id, "vc4hdmi"))) continue;
            rank = strcmp(id, "vc4hdmi0") ? 1 : 0;
        } else if (!strcasecmp(output, "jack")) {
            /* Check the driver too: a USB headset can also be named Headphones. */
            if (usb || strncmp(id, "Headphones", 10) || (id[10] && id[10] != '_')) continue;
            char driver[PATH_MAX];
            snprintf(path, sizeof(path), "%s/card%u/device/driver", sound_root, card);
            ssize_t length = readlink(path, driver, sizeof(driver) - 1);
            if (length < 0) continue;
            driver[length] = 0;
            const char *base = strrchr(driver, '/');
            if (strcmp(base ? base + 1 : driver, "bcm2835-audio")) continue;
        } else if (strcasecmp(output, "usb") || !usb) continue;
        if (!*best || rank < best_rank || (rank == best_rank &&
            (strcmp(id, best) < 0 || (!strcmp(id, best) && pcm < best_pcm)))) {
            snprintf(best, sizeof(best), "%s", id);
            best_pcm = pcm;
            best_rank = rank;
        }
    }
    globfree(&paths);
    return *best && snprintf(device, size, "plughw:CARD=%s,DEV=%u", best, best_pcm) < (int)size;
}

void pi_dos_audio_prepare(const char *config)
{
    char output[80] = "hdmi", device[128];
    if (!pi_dos_config_value(config, "pidos", "audio_output", output, sizeof(output)))
        snprintf(output, sizeof(output), "hdmi");
    else if (strcasecmp(output, "hdmi") && strcasecmp(output, "jack") && strcasecmp(output, "usb")) {
        fprintf(stderr, "Pi-DOS: invalid audio_output; using HDMI\n");
        snprintf(output, sizeof(output), "hdmi");
    }
    /* Allow built-in and USB drivers up to five seconds to register. */
    for (int attempt = 0; attempt <= 50; ++attempt) {
        if (pi_dos_audio_find(output, "/sys/class/sound", "/proc/asound", device, sizeof(device))) {
            setenv("AUDIODEV", device, 1);
            fprintf(stderr, "Pi-DOS: audio output %s (%s)\n", output, device);
            return;
        }
        if (attempt == 50) break;
        struct timespec delay = {0, 100000000};
        while (nanosleep(&delay, &delay) && errno == EINTR) {}
    }
    /* An unavailable choice must never redirect audio to a different output. */
    setenv("AUDIODEV", "plughw:CARD=PiDOSMissing,DEV=0", 1);
    fprintf(stderr, "Pi-DOS: audio output %s unavailable; audio disabled for this boot\n", output);
}
