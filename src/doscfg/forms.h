#ifndef PIDOSCFG_FORMS_H
#define PIDOSCFG_FORMS_H
#include "config.h"
enum { FIELD_CHOICE, FIELD_CARD, FIELD_CHECKBOX, FIELD_TEXT, FIELD_VOLUME, FIELD_VIDEO, FIELD_VOODOO,
       FIELD_MIDI, FIELD_MIDI_SETTINGS, FIELD_MIDI_PORT };
typedef struct {
    int key, type;
    const char *label, *choices, *catalog, *help;
} Field;
extern const char *page_names[PAGE_COUNT];
#define MEMORY_COUNT 8
extern const char *memory_sizes[MEMORY_COUNT];
int form_fields(int page, Field *fields);
int form_midi_fields(Field *fields);
#endif
