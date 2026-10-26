/* DOSBox-X built-in commands provide live control without a host-side API. */
#include <dos.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "config.h"
#include "live.h"
#define CHANNELS (K_VOL_MIDI-K_VOL_MASTER+1)
static const char *channels[CHANNELS]={"MASTER","SB","FM","GUS","CMS","SPKR","TANDY","TANDYDAC","DISNEY","SYNTH"};
static char midi_channel[8]="SYNTH";
static char runtime[K_COUNT][CFG_VALUE_SIZE];
static unsigned char have[K_COUNT],active[CHANNELS],seeded[CHANNELS];
static char original[CHANNELS][32],applied[CHANNELS][32];
static int state,matching;

static int temporary(char *path) {
 int location,i,handle; unsigned int id=(unsigned int)_psp;
 for(location=0;location<2;++location) for(i=0;i<32;++i) {
  sprintf(path,location?"C:\\MX%04X.TMP":"C:\\TEMP\\MX%04X.TMP",(id+i)&65535U);
  if(!_dos_creatnew(path,0,&handle)) { _dos_close(handle); return 1; }
 }
 return 0;
}
static void trim(char *value) {
 char *p=value; unsigned int length;
 while(*p==' ' || *p=='\t') ++p;
 if(p!=value) memmove(value,p,strlen(p)+1);
 length=(unsigned int)strlen(value);
 while(length && strchr(" \t\r\n",value[length-1])) value[--length]=0;
}
static int snapshot(void) {
 static const char *sections[]={"dosbox","sblaster","gus","speaker","midi"};
 char path[40],command[120],line[256],name[32],*equals; FILE *file;
 int i,key,left,right;
 if(!temporary(path)) return 0;
 for(i=0;i<5;++i) {
  sprintf(command,"Z:\\SYSTEM\\CONFIG.COM -ALL -GET %s %s%s",sections[i],i?">>":">",path);
  if(system(command)==-1) { remove(path); return 0; }
 }
 file=fopen(path,"r");
 if(!file) { remove(path); return 0; }
 while(fgets(line,sizeof(line),file)) {
  equals=strchr(line,'='); if(!equals) continue;
  *equals++=0; trim(line); trim(equals);
  if(strlen(equals)>=CFG_VALUE_SIZE) continue;
  for(key=K_MACHINE;key<K_COUNT;++key) if(!stricmp(line,cfg_specs[key].key)) {
   strcpy(runtime[key],equals); have[key]=1;
  }
 }
 fclose(file);
 sprintf(command,"Z:\\SYSTEM\\MIXER.COM >%s",path);
 if(system(command)==-1) { remove(path); return 0; }
 file=fopen(path,"r");
 if(!file) { remove(path); return 0; }
 while(fgets(line,sizeof(line),file)) {
  if(sscanf(line,"%31s %d:%d",name,&left,&right)!=3 || left<0 || right<0 || left>999 || right>999) continue;
  if(!stricmp(name,"MT32")) strcpy(midi_channel,"MT32");
  for(i=0;i<CHANNELS;++i) if(!stricmp(name,channels[i]) || (i==K_VOL_MIDI-K_VOL_MASTER && !stricmp(name,"MT32"))) {
   sprintf(original[i],"%d:%d",left,right); strcpy(applied[i],original[i]); active[i]=1;
  }
 }
 fclose(file); remove(path);
 return active[0] && have[K_SB_TYPE];
}
static int matches(int key) {
 return have[key] && !stricmp(cfg_get(key),runtime[key]);
}
static int hardware_matches(void) {
 int key,card=cfg_card();
 char midi_config[CFG_VALUE_SIZE+16];
 static const int common[]={K_MACHINE,K_SB_TYPE,K_OPL,K_CMS,K_GUS,K_TANDY,K_SPEAKER};
 for(key=0;key<(int)(sizeof(common)/sizeof(common[0]));++key) if(!matches(common[key])) return 0;
 if(card==CARD_CMS && !matches(K_SB_PORT)) return 0;
 if(card==CARD_SB2 || card==CARD_PRO2 || card==CARD_SB16) {
  for(key=K_SB_PORT;key<=K_SB_DMA;++key) if(!matches(key)) return 0;
  if(card==CARD_SB16 && !matches(K_SB_HDMA)) return 0;
 }
 if(card==CARD_GUS) for(key=K_GUS_TYPE;key<=K_GUS_RAM;++key) if(!matches(key)) return 0;
 if(!matches(K_MPU) || !matches(K_MIDI_DEVICE)) return 0;
 if(stricmp(cfg_get(K_MPU),"none")) {
  if(!matches(K_MPU_BASE) || !matches(K_MPU_IRQ)) return 0;
  sprintf(midi_config,"%s%s",cfg_get(K_MIDI_CONFIG),
   !stricmp(cfg_get(K_MIDI_DEVICE),"alsa") && !stricmp(cfg_get(K_MIDI_DELAY),"true")?" delaysysex":"");
  if(!have[K_MIDI_CONFIG] || stricmp(midi_config,runtime[K_MIDI_CONFIG])) return 0;
  if(!stricmp(cfg_get(K_MIDI_DEVICE),"mt32") && (!matches(K_MT32_MODEL) || !matches(K_MT32_DIR))) return 0;
  if(!stricmp(cfg_get(K_MIDI_DEVICE),"alsa") && cfg_is_dirty(K_MIDI_DELAY)) return 0;
 }
 return card>=0;
}
static int apply(int channel,const char *value) {
 char command[100];
 sprintf(command,"Z:\\SYSTEM\\MIXER.COM /NOSHOW %s %s",channel==K_VOL_MIDI-K_VOL_MASTER?midi_channel:channels[channel],value);
 if(system(command)==-1) return 0;
 strcpy(applied[channel],value); return 1;
}
void live_cancel(void) {
 int i;
 if(state==1) for(i=0;i<CHANNELS;++i)
  if(active[i] && strcmp(applied[i],original[i])) apply(i,original[i]);
}
void live_update(int mixer_page) {
 int i,key;
 if(!state && mixer_page) state=snapshot()?1:-1;
 if(state!=1) return;
 matching=hardware_matches();
 if(!matching) { live_cancel(); return; }
 for(i=0;i<CHANNELS;++i) if(active[i]) {
  key=K_VOL_MASTER+i;
  if(!seeded[i]) { cfg_seed(key,original[i]); seeded[i]=1; }
  if(cfg_is_dirty(key) && strcmp(applied[i],cfg_get(key))) {
   if(!apply(i,cfg_get(key))) { live_cancel(); state=-1; return; }
  }
 }
}
const char *live_status(void) {
 if(state==1 && matching) return "LIVE: active channels apply now; Cancel restores starting levels.";
 if(state==1) return "AFTER REBOOT: selected sound hardware differs from the running setup.";
 return "AFTER REBOOT: live mixer unavailable; settings can still be saved.";
}
