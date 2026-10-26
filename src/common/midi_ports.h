#ifndef PI_DOS_MIDI_PORTS_H
#define PI_DOS_MIDI_PORTS_H
/* Shared by PID 1 and the ALSA MIDI backend. No session client numbers in IDs. */
#include <alsa/asoundlib.h>
#include <ctype.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
typedef struct { char id[23], label[80]; int client, port; } PiDosMidiPort;
typedef void (*PiDosMidiVisitor)(const PiDosMidiPort *, void *);
static inline int pidos_midi_read(const char *path, char *value, size_t size) {
    FILE *f = fopen(path, "r");
    if (!f) return 0;
    int ok = fgets(value, (int)size, f) != NULL;
    fclose(f);
    if (ok) value[strcspn(value, "\r\n")] = 0;
    return ok && *value;
}
static inline void pidos_midi_identity(int card, const char *client, const char *port,
                                      int number, char id[23]) {
    char link[80], resolved[PATH_MAX], physical[PATH_MAX], usb[300] = "";
    const char *device = client;
    if (card >= 0) {
        snprintf(link, sizeof(link), "/sys/class/sound/card%d/device", card);
        if (realpath(link, resolved)) {
            strcpy(physical, resolved); device = resolved;
            for (int level = 0; level < 6; ++level) {
                char path[PATH_MAX + 32], vendor[16], product[16], serial[240];
                snprintf(path, sizeof(path), "%s/idVendor", physical);
                if (pidos_midi_read(path, vendor, sizeof(vendor))) {
                    snprintf(path, sizeof(path), "%s/idProduct", physical);
                    if (!pidos_midi_read(path, product, sizeof(product))) break;
                    snprintf(path, sizeof(path), "%s/serial", physical);
                    if (pidos_midi_read(path, serial, sizeof(serial))) {
                        snprintf(usb, sizeof(usb), "%s:%s:%s", vendor, product, serial);
                        device = usb;
                    }
                    break;
                }
                char *slash = strrchr(physical, '/');
                if (!slash || slash == physical) break;
                *slash = 0;
            }
        }
    }
    char suffix[32]; snprintf(suffix, sizeof(suffix), ":%d", number);
    const char *parts[3] = {device, port, suffix};
    uint64_t hash = UINT64_C(14695981039346656037);
    for (int i = 0; i < 3; ++i) {
        for (const unsigned char *p = (const unsigned char *)parts[i]; *p; ++p)
            hash = (hash ^ *p) * UINT64_C(1099511628211);
        hash = (hash ^ 0xff) * UINT64_C(1099511628211);
    }
    snprintf(id, 23, "pidos:%016llx", (unsigned long long)hash);
}
static inline int pidos_midi_ports(PiDosMidiVisitor visit, void *data) {
    snd_seq_t *seq = NULL;
    if (snd_seq_open(&seq, "default", SND_SEQ_OPEN_OUTPUT, SND_SEQ_NONBLOCK) < 0) return -1;
    snd_seq_client_info_t *client = NULL; snd_seq_port_info_t *port = NULL;
    if (snd_seq_client_info_malloc(&client) < 0 || snd_seq_port_info_malloc(&port) < 0) {
        if (client) snd_seq_client_info_free(client);
        snd_seq_close(seq); return -1;
    }
    int count = 0;
    snd_seq_client_info_set_client(client, -1);
    while (snd_seq_query_next_client(seq, client) >= 0) {
        int client_id = snd_seq_client_info_get_client(client);
        snd_seq_port_info_set_client(port, client_id); snd_seq_port_info_set_port(port, -1);
        while (snd_seq_query_next_port(seq, port) >= 0) {
            unsigned int caps = snd_seq_port_info_get_capability(port);
            unsigned int wanted = SND_SEQ_PORT_CAP_WRITE | SND_SEQ_PORT_CAP_SUBS_WRITE;
            if ((caps & wanted) != wanted || !(snd_seq_port_info_get_type(port) & SND_SEQ_PORT_TYPE_MIDI_GENERIC)) continue;
            PiDosMidiPort found;
            found.client = client_id; found.port = snd_seq_port_info_get_port(port);
            pidos_midi_identity(snd_seq_client_info_get_card(client), snd_seq_client_info_get_name(client),
                                snd_seq_port_info_get_name(port), found.port, found.id);
            snprintf(found.label, sizeof(found.label), "%.28s / %.28s [%d:%d]", snd_seq_client_info_get_name(client), snd_seq_port_info_get_name(port), found.client, found.port);
            for (unsigned char *p = (unsigned char *)found.label; *p; ++p) if (*p < 32 || *p > 126) *p = '?';
            if (visit) visit(&found, data);
            ++count;
        }
    }
    snd_seq_client_info_free(client); snd_seq_port_info_free(port); snd_seq_close(seq);
    return count;
}
typedef struct { const char *id; int client, port, matches; } PiDosMidiLookup;
static inline void pidos_midi_match(const PiDosMidiPort *port, void *data) {
    PiDosMidiLookup *lookup = (PiDosMidiLookup *)data;
    if (!strcasecmp(port->id, lookup->id)) {
        lookup->client = port->client; lookup->port = port->port; ++lookup->matches;
    }
}
static inline int pidos_midi_resolve(const char *id, int *client, int *port) {
    PiDosMidiLookup lookup = {id, -1, -1, 0};
    if (pidos_midi_ports(pidos_midi_match, &lookup) < 0 || lookup.matches != 1) return 0;
    *client = lookup.client; *port = lookup.port; return 1;
}
#endif
