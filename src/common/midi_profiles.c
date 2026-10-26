#include <ctype.h>
#include "midi_profiles.h"
const MidiProfile midi_profiles[MIDI_PROFILE_COUNT] = {
 {"none", "Disabled", "none", "", "auto"},
 {"sc55", "Roland SC-55", "synth", "SC55.SF2", "auto"},
 {"roland-gm", "Roland GM", "synth", "GMROLAND.SF2", "auto"},
 {"mt32", "Roland MT-32", "mt32", "MT32", "mt32"},
 {"cm32l", "Roland CM-32L", "mt32", "CM32L", "cm32l"},
 {"external", "External USB MIDI", "alsa", "", "auto"}
};
int midi_profile_find(const char *id) {
 int i;
 if(!id) return -1;
 for(i=0;i<MIDI_PROFILE_COUNT;++i) {
  const char *a=id,*b=midi_profiles[i].id;
  while(*a && *b && tolower((unsigned char)*a)==tolower((unsigned char)*b)) { ++a; ++b; }
  if(!*a && !*b) return i;
 }
 return -1;
}
