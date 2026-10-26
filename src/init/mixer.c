/* Persisted DOSBox mixer percentages; never interpolate unchecked DOS input. */
#include "mixer.h"
#include "config.h"
#include <ctype.h>
#include <stdlib.h>

static int percentage(const char **text, unsigned long *result)
{
    if (!isdigit((unsigned char)**text)) return 0;
    char *end;
    unsigned long n = strtoul(*text, &end, 10);
    if (n > 200) return 0;
    *text = end;
    *result = n;
    return 1;
}

void pi_dos_mixer(FILE *out, const char *config)
{
    static const struct { const char *key, *channel; } channels[] = {
        {"mixer_master", "MASTER"}, {"mixer_sb", "SB"}, {"mixer_fm", "FM"},
        {"mixer_gus", "GUS"}, {"mixer_cms", "CMS"}, {"mixer_speaker", "SPKR"},
        {"mixer_tandy", "TANDY"}, {"mixer_tandydac", "TANDYDAC"},
        {"mixer_disney", "DISNEY"},
        {"mixer_midi", "SYNTH"}, {"mixer_midi", "MT32"}
    };
    for (unsigned int i = 0; i < sizeof(channels) / sizeof(channels[0]); ++i) {
        char value[32];
        if (!pi_dos_config_value(config, "pidos", channels[i].key, value, sizeof(value))) continue;
        const char *p = value;
        unsigned long left, right;
        if (!percentage(&p, &left)) continue;
        right = left;
        if (*p == ':') { ++p; if (!percentage(&p, &right)) continue; }
        if (*p) continue;
        fprintf(out, "Z:\\SYSTEM\\MIXER.COM /NOSHOW %s %lu:%lu\n",
                channels[i].channel, left, right);
    }
}
