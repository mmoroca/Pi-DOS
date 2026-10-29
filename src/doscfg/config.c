/* Portable C89 configuration editor; also built for 8086 real-mode DOS. */
#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef __WATCOMC__
#include <dos.h>
#endif
#include "config.h"
#define PIDOS_INPUT_OPTIONS_VALIDATE
#include "input_options.h"
#define CFG_CAPACITY 24576
#define S(section,key,value,page) {section,key,value,page}
const CfgSpec cfg_specs[K_COUNT] = {
 S("cpu","cputype","auto",PAGE_CPU), S("cpu","core","dynamic_rec",PAGE_CPU),
 S("cpu","cycles","auto",PAGE_CPU),
 S("dosbox","memsize","16",PAGE_MEMORY), S("dosbox","memsizekb","0",PAGE_MEMORY),
 S("dosbox","machine","svga_s3",PAGE_VIDEO),
 S("sblaster","sbtype","sb16",PAGE_SOUND), S("sblaster","sbbase","220",PAGE_SOUND),
 S("sblaster","irq","7",PAGE_SOUND), S("sblaster","dma","1",PAGE_SOUND),
 S("sblaster","hdma","5",PAGE_SOUND), S("sblaster","oplmode","auto",PAGE_SOUND),
 S("sblaster","cms","off",PAGE_SOUND),
 S("gus","gus","false",PAGE_SOUND), S("gus","gustype","classic",PAGE_SOUND),
 S("gus","gusbase","240",PAGE_SOUND), S("gus","gusirq","5",PAGE_SOUND),
 S("gus","gusdma","3",PAGE_SOUND), S("gus","gusmemsize","1024",PAGE_SOUND),
 S("gus","ultradir","C:\\ULTRASND",PAGE_SOUND),
 S("speaker","tandy","off",PAGE_SOUND), S("speaker","pcspeaker","true",PAGE_SOUND),
 S("pidos","mixer_master","100:100",PAGE_MIXER), S("pidos","mixer_sb","100:100",PAGE_MIXER),
 S("pidos","mixer_fm","100:100",PAGE_MIXER), S("pidos","mixer_gus","100:100",PAGE_MIXER),
 S("pidos","mixer_cms","100:100",PAGE_MIXER), S("pidos","mixer_speaker","100:100",PAGE_MIXER),
 S("pidos","mixer_tandy","100:100",PAGE_MIXER), S("pidos","mixer_tandydac","100:100",PAGE_MIXER),
 S("pidos","mixer_disney","100:100",PAGE_MIXER), S("pidos","mixer_midi","100:100",PAGE_MIXER),
 S("dos","keyboardlayout","sp",PAGE_SYSTEM), S("config","country","34",PAGE_SYSTEM),
 S("pidos","timezone","Europe/Madrid",PAGE_SYSTEM),
 S("voodoo","voodoo_card","software",PAGE_VIDEO),
 S("voodoo","voodoo_maxmem","true",PAGE_VIDEO), S("voodoo","glide","false",PAGE_VIDEO),
 S("pidos","diagnostics","false",PAGE_SYSTEM),
 S("pidos","midi_profile","none",PAGE_SOUND),
 S("midi","mpu401","none",PAGE_SOUND), S("midi","mpubase","330",PAGE_SOUND),
 S("midi","mpuirq","-1",PAGE_SOUND), S("midi","mididevice","none",PAGE_SOUND),
 S("midi","midiconfig","",PAGE_SOUND), S("midi","mt32.model","auto",PAGE_SOUND),
 S("midi","mt32.romdir","",PAGE_SOUND), S("pidos","midi_port","",PAGE_SOUND),
 S("pidos","midi_delaysysex","false",PAGE_SOUND),
 S("pidos","monitor","lcd",PAGE_SYSTEM),
 S("pidos","lcd_mode","1024x768",PAGE_SYSTEM),
 S("pidos","lcd_aspect","4:3",PAGE_SYSTEM),
 S("joystick","joysticktype","2axis",PAGE_INPUT),
 S("pidos","controller_layout","auto",PAGE_INPUT),
 S("pidos","controller_direction","both",PAGE_INPUT),
 S("pidos","controller_swap_buttons","false",PAGE_INPUT),
 S("joystick","joy1deadzone1","0.25",PAGE_INPUT),
 S("joystick","joy1deadzone2","0.25",PAGE_INPUT),
 S("joystick","joy2deadzone1","0.25",PAGE_INPUT),
 S("joystick","timed","true",PAGE_INPUT),
 S("joystick","autofire","false",PAGE_INPUT),
 S("pidos","shortcut_reboot","Ctrl+Alt+Delete",PAGE_SYSTEM),
 S("pidos","shortcut_poweroff","none",PAGE_SYSTEM),
 S("pidos","shortcut_pause","none",PAGE_SYSTEM)
};
const char *card_names[CARD_COUNT] = {"None", "Tandy sound", "AdLib",
 "Creative Game Blaster / CMS", "Sound Blaster 2.0", "Sound Blaster Pro 2",
 "Sound Blaster 16 (default)", "Gravis Ultrasound Classic"};
char cfg_error[160], cfg_type[CFG_VALUE_SIZE], cfg_core[CFG_VALUE_SIZE], cfg_cycles[CFG_VALUE_SIZE];
static char values[K_COUNT][CFG_VALUE_SIZE], contents[CFG_CAPACITY + 1];
static unsigned char dirty[K_COUNT];
static unsigned int content_size;
static int file_exists;
static int same(const char *a, const char *b) {
 while (*a && *b) if (tolower((unsigned char)*a++) != tolower((unsigned char)*b++)) return 0;
 return *a == *b;
}
static int one_of(const char *value, const char *list) {
 char word[32]; unsigned int n;
 while (*list) {
  n=0; while (*list && *list!='|') { if (n<sizeof(word)-1) word[n++]=*list; ++list; }
  word[n]=0; if (same(value,word)) return 1; if (*list) ++list;
 }
 return 0;
}
static int number(const char *text, long min, long max, int base) {
 char *end; long n;
 if (!*text || isspace((unsigned char)*text)) return 0;
 n=strtol(text,&end,base); return !*end && n>=min && n<=max;
}
static void sync_cpu(void) {
 strcpy(cfg_type,values[K_CPU_TYPE]); strcpy(cfg_core,values[K_CPU_CORE]); strcpy(cfg_cycles,values[K_CPU_CYCLES]);
}
const char *cfg_get(int key) { return key>=0 && key<K_COUNT ? values[key] : ""; }
int cfg_changed(void) { int i; for(i=0;i<K_COUNT;++i) if(dirty[i]) return 1; return 0; }
int cfg_is_dirty(int key) { return key>=0 && key<K_COUNT && dirty[key]; }
/* Display the running mixer level without silently persisting it. */
void cfg_seed(int key,const char *value) {
 if(key>=K_VOL_MASTER && key<=K_VOL_MIDI && !dirty[key] && strlen(value)<CFG_VALUE_SIZE)
  strcpy(values[key],value);
}
int cfg_set(int key, const char *value) {
 unsigned int i; char copy[CFG_VALUE_SIZE], *colon; int valid=1;
 if(key<0 || key>=K_COUNT || strlen(value)>=CFG_VALUE_SIZE) valid=0;
 if(!*value && key!=K_MIDI_CONFIG && key!=K_MT32_DIR && key!=K_MIDI_PORT) valid=0;
 if(valid) for(i=0;value[i];++i) if((unsigned char)value[i]<32 || (unsigned char)value[i]>126) valid=0;
 if(!valid) { strcpy(cfg_error,"Invalid or overlong configuration value."); return 0; }
 switch(key) {
 case K_JOY_TYPE: valid=one_of(value,"none|auto|2axis|4axis|4axis_2|fcs|ch"); break;
 case K_CONTROLLER_LAYOUT: valid=one_of(value,"auto|raw"); break;
 case K_CONTROLLER_DIRECTION: valid=one_of(value,"both|stick|dpad"); break;
 case K_JOY_DEADZONE: case K_JOY_DEADZONE2: case K_JOY_DEADZONE3:
  valid=value[0]=='0' && (!value[1] || (value[1]=='.' && value[2]));
  for(i=2;valid && value[1] && value[i];++i) if(!isdigit((unsigned char)value[i])) valid=0;
  break;
 case K_CONTROLLER_SWAP: case K_JOY_TIMED: case K_JOY_AUTOFIRE:
  valid=one_of(value,"true|false"); break;
 case K_SHORTCUT_REBOOT: case K_SHORTCUT_POWEROFF: case K_SHORTCUT_PAUSE:
  valid=pidos_shortcut_key(value)>=0; break;
 case K_MONITOR: valid=one_of(value,"lcd|crt"); break;
 case K_LCD_MODE: valid=lcd_mode_find(value)>=0; break;
 case K_LCD_ASPECT: valid=one_of(value,"4:3|original"); break;
 case K_RAM: valid=one_of(value,"1|2|4|8|16|32|64|128"); break;
 case K_RAM_KB: valid=!strcmp(value,"0"); break;
 case K_MACHINE: valid=video_profile_find(value)>=0; break;
 case K_VOODOO: valid=one_of(value,"software|false"); break;
 case K_MIDI_PROFILE: valid=midi_profile_find(value)>=0; break;
 case K_MPU: valid=one_of(value,"none|intelligent|uart"); break;
 case K_MPU_BASE: valid=one_of(value,"0|300|310|320|330|332|334|336|340|360"); break;
 case K_MPU_IRQ: valid=number(value,-1,15,10); break;
 case K_MIDI_DEVICE: valid=one_of(value,"none|synth|mt32|alsa"); break;
 case K_MT32_MODEL: valid=one_of(value,"auto|mt32|cm32l"); break;
 case K_MIDI_PORT:
  valid=!*value || (strlen(value)==22 && !memcmp(value,"pidos:",6));
  if(*value) for(i=6;valid && value[i];++i) if(!isxdigit((unsigned char)value[i])) valid=0;
  break;
 case K_SB_TYPE: valid=one_of(value,"none|gb|sb2.0|sbpro2|sb16"); break;
 case K_SB_PORT: valid=one_of(value,"220|240|260|280|2a0|2c0|2e0"); break;
 case K_GUS_PORT: valid=one_of(value,"210|220|230|240|250|260|280|2a0|2c0|2e0|300"); break;
 case K_SB_IRQ: case K_GUS_IRQ: valid=one_of(value,"3|5|7|9|10|11|12"); break;
 case K_SB_DMA: valid=one_of(value,"0|1|3"); break;
 case K_SB_HDMA: valid=one_of(value,"5|6|7"); break;
 case K_GUS_DMA: valid=one_of(value,"0|1|3|5|6|7"); break;
 case K_GUS_RAM: valid=one_of(value,"256|512|768|1024"); break;
 case K_GUS_TYPE: valid=same(value,"classic"); break;
 case K_GUS: case K_SPEAKER: case K_VOODOO_MEM: case K_GLIDE: case K_DIAGNOSTICS: case K_MIDI_DELAY:
  valid=one_of(value,"true|false"); break;
 case K_TANDY: case K_CMS: valid=one_of(value,"on|off|auto"); break;
 case K_OPL: valid=one_of(value,"none|auto|opl2|dualopl2|opl3"); break;
 case K_KEYBOARD:
  for(i=0;value[i];++i) if(!isalnum((unsigned char)value[i]) && value[i]!='_' && value[i]!='-') valid=0;
  break;
 case K_COUNTRY:
  strcpy(copy,value); colon=strchr(copy,','); if(colon) { *colon++=0; valid=number(colon,1,65535,10); }
  valid=valid && number(copy,1,999,10); break;
 case K_TIMEZONE:
  if(value[0]=='/' || strlen(value)>63) valid=0;
  for(i=0;value[i];++i) if(!isalnum((unsigned char)value[i]) && !strchr("/_+-",value[i])) valid=0;
  break;
 default:
  if(key>=K_VOL_MASTER && key<=K_VOL_MIDI) {
   strcpy(copy,value); colon=strchr(copy,':');
   if(colon) { *colon++=0; valid=number(colon,0,200,10); }
   valid=valid && number(copy,0,200,10);
  }
  break;
 }
 if(!valid) { strcpy(cfg_error,"Value is outside the supported range. Check the field's help."); return 0; }
 if(strcmp(values[key],value)) { strcpy(values[key],value); dirty[key]=1; }
 /* Selecting a monitor or LCD option materializes the managed profile. */
 if(key>=K_MONITOR && key<=K_LCD_ASPECT) dirty[K_MONITOR]=1;
 if(key==K_JOY_DEADZONE) {
  strcpy(values[K_JOY_DEADZONE2],value); strcpy(values[K_JOY_DEADZONE3],value);
  dirty[K_JOY_DEADZONE2]=dirty[K_JOY_DEADZONE3]=1;
 }
 sync_cpu(); cfg_error[0]=0; return 1;
}
int cfg_profile(void) { return cpu_profile_find(values[K_CPU_TYPE],values[K_CPU_CORE],values[K_CPU_CYCLES]); }
int cfg_select_cpu(int profile) {
 const CpuProfile *p;
 if(profile<0 || profile>=PROFILE_COUNT) { strcpy(cfg_error,"Select a CPU profile first."); return 0; }
 p=&cpu_profiles[profile];
 return cfg_set(K_CPU_TYPE,p->type) && cfg_set(K_CPU_CORE,p->core) && cfg_set(K_CPU_CYCLES,p->cycles);
}
int cfg_card(void) {
 int gus=same(values[K_GUS],"true"), tandy=same(values[K_TANDY],"on") ||
  (same(values[K_TANDY],"auto") && same(values[K_MACHINE],"tandy"));
 const char *sb=values[K_SB_TYPE];
 if(gus && !tandy && same(sb,"none") && same(values[K_OPL],"none")) return CARD_GUS;
 if(tandy && !gus && same(sb,"none") && same(values[K_OPL],"none")) return CARD_TANDY;
 if(gus || tandy) return -1;
 if(same(sb,"none")) {
  if(same(values[K_OPL],"none")) return CARD_NONE;
  if(same(values[K_OPL],"opl2")) return CARD_ADLIB;
  return -1;
 }
 if(same(sb,"gb")) return CARD_CMS;
 if(one_of(sb,"sb2|sb2.0|sb2.01")) return CARD_SB2;
 if(same(sb,"sbpro2")) return CARD_PRO2;
 if(same(sb,"sb16")) return CARD_SB16;
 return -1;
}
void cfg_select_card(int card) {
 const char *sb="none", *opl="none";
 if(card<0 || card>=CARD_COUNT) return;
 if(card==CARD_SB2) { sb="sb2.0"; opl="opl2"; }
 if(card==CARD_PRO2) { sb="sbpro2"; opl="opl3"; }
 if(card==CARD_SB16) { sb="sb16"; opl="auto"; }
 if(card==CARD_CMS) sb="gb";
 if(card==CARD_ADLIB) opl="opl2";
 cfg_set(K_SB_TYPE,sb); cfg_set(K_OPL,opl);
 cfg_set(K_CMS,card==CARD_CMS?"on":"off");
 cfg_set(K_GUS,card==CARD_GUS?"true":"false");
 if(card==CARD_GUS) cfg_set(K_GUS_TYPE,"classic");
 cfg_set(K_TANDY,card==CARD_TANDY?"on":"off");
 /* PC speaker and MIDI deliberately remain independent. */
}
void cfg_reset_page(int page) {
 int i;
 if(page==PAGE_CPU) { cfg_select_cpu(cpu_profile_default()); return; }
 for(i=0;i<K_COUNT;++i) if(cfg_specs[i].page==page) cfg_set(i,cfg_specs[i].default_value);
 if(page==PAGE_SOUND) cfg_select_midi(MIDI_NONE);
}
int cfg_midi(void) {
 int profile=midi_profile_find(cfg_get(K_MIDI_PROFILE));
 if(profile==MIDI_NONE && !same(cfg_get(K_MPU),"none")) return -1;
 return profile;
}
void cfg_select_midi(int profile) {
 char path[CFG_VALUE_SIZE]; int key;
 const MidiProfile *p;
 if(profile<0 || profile>=MIDI_PROFILE_COUNT) return;
 p=&midi_profiles[profile]; cfg_set(K_MIDI_PROFILE,p->id);
 cfg_set(K_MPU,profile==MIDI_NONE?"none":same(cfg_get(K_MPU),"uart")?"uart":"intelligent");
 cfg_set(K_MIDI_DEVICE,p->device); cfg_set(K_MT32_MODEL,p->model);
 path[0]=0;
 if(profile==MIDI_SC55 || profile==MIDI_ROLAND_GM) sprintf(path,"%s/%s",MIDI_ASSET_ROOT,p->asset);
 if(profile==MIDI_EXTERNAL) strcpy(path,cfg_get(K_MIDI_PORT));
 cfg_set(K_MIDI_CONFIG,path);
 path[0]=0;
 if(profile==MIDI_MT32 || profile==MIDI_CM32L) sprintf(path,"%s/%s",MIDI_ASSET_ROOT,p->asset);
 cfg_set(K_MT32_DIR,path);
 /* Explicit profile selection also materializes absent legacy defaults. */
 for(key=K_MIDI_PROFILE;key<=K_MT32_DIR;++key) dirty[key]=1;
}
void cfg_toggle_voodoo(void) {
 cfg_set(K_VOODOO,same(cfg_get(K_VOODOO),"false")?"software":"false");
 cfg_set(K_VOODOO_MEM,"true"); cfg_set(K_GLIDE,"false");
 /* Materialize the whole preset, including absent keys in existing files. */
 dirty[K_VOODOO]=dirty[K_VOODOO_MEM]=dirty[K_GLIDE]=1;
}
static void span(unsigned int start,unsigned int end,char *out,unsigned int size) {
 unsigned int n;
 while(start<end && isspace((unsigned char)contents[start])) ++start;
 while(end>start && isspace((unsigned char)contents[end-1])) --end;
 n=end-start; if(n>=size) n=size-1; memcpy(out,contents+start,n); out[n]=0;
}
/* Return a setting index or -1, updating the current section as necessary. */
static int parse_line(unsigned int start,unsigned int end,char *section,unsigned int *equals) {
 unsigned int i; int key; char name[CFG_VALUE_SIZE];
 while(start<end && isspace((unsigned char)contents[start])) ++start;
 if(end-start>=14 && !memcmp(contents+start,"#DOSBOX-X-ADV:",14)) start+=14;
 else if(end-start>=10 && !memcmp(contents+start,"#DOSBOX-X:",10)) start+=10;
 while(start<end && isspace((unsigned char)contents[start])) ++start;
 if(start==end || strchr("#;%",contents[start])) return -1;
 if(contents[start]=='[') {
  for(i=start+1;i<end && contents[i]!=']';++i) {}
  if(i<end) span(start+1,i,section,CFG_VALUE_SIZE);
  return -1;
 }
 for(i=start;i<end && contents[i]!='=';++i) {}
 if(i==end) return -1;
 span(start,i,name,sizeof(name)); *equals=i;
 for(key=0;key<K_COUNT;++key) if(same(section,cfg_specs[key].section) && same(name,cfg_specs[key].key)) return key;
 return -1;
}
static int load_contents(const char *path) {
 FILE *input; int extra,failed;
 cfg_error[0]=0; content_size=0; file_exists=0;
 input=fopen(path,"rb");
 if(!input) { if(errno==ENOENT) return 1; strcpy(cfg_error,"Cannot read the configuration file."); return 0; }
 file_exists=1;
 content_size=(unsigned int)fread(contents,1,CFG_CAPACITY,input); extra=fgetc(input); failed=ferror(input);
 if(fclose(input)) failed=1;
 if(failed || extra!=EOF || memchr(contents,0,content_size)) {
  strcpy(cfg_error,"Cannot edit: file is unreadable, binary, or larger than 24 KiB."); return 0;
 }
 contents[content_size]=0; return 1;
}
int cfg_load(const char *path) {
 unsigned int start,end,equals; int key,i,monitor_found=0; char section[CFG_VALUE_SIZE]="";
 const CpuProfile *p=&cpu_profiles[cpu_profile_default()];
 for(i=0;i<K_COUNT;++i) { strcpy(values[i],cfg_specs[i].default_value); dirty[i]=0; }
 strcpy(values[K_CPU_TYPE],p->type); strcpy(values[K_CPU_CORE],p->core); strcpy(values[K_CPU_CYCLES],p->cycles);
 if(!load_contents(path)) return 0;
 for(start=0;start<content_size;start=end) {
  for(end=start;end<content_size && contents[end]!='\n';++end) {}
  key=parse_line(start,end,section,&equals);
  if(key==K_MONITOR) monitor_found=1;
  if(key>=0) span(equals+1,end,values[key],CFG_VALUE_SIZE);
  if(end<content_size) ++end;
 }
 /* Existing files without a monitor setting retain their previous CRT setup. */
 if(file_exists && !monitor_found) strcpy(values[K_MONITOR],"crt");
 sync_cpu(); return 1;
}
static int write_span(FILE *out,unsigned int start,unsigned int end) {
 return fwrite(contents+start,1,end-start,out)==end-start;
}
static int write_config(FILE *out) {
 unsigned int start,end,next,equals; int key,i,j; unsigned char seen[K_COUNT];
 char section[CFG_VALUE_SIZE]="";
 memset(seen,0,sizeof(seen));
 for(start=0;start<content_size;start=next) {
  for(end=start;end<content_size && contents[end]!='\n';++end) {}
  next=end<content_size?end+1:end; key=parse_line(start,end,section,&equals);
  if(key>=0 && dirty[key]) {
   seen[key]=1;
   if(!write_span(out,start,equals+1) || fprintf(out,"%s\r\n",values[key])<0) return 0;
  } else if(!write_span(out,start,next)) return 0;
 }
 for(i=0;i<K_COUNT;++i) if(dirty[i] && !seen[i]) {
  if(fputs("\r\n",out)==EOF || fprintf(out,"[%s]\r\n",cfg_specs[i].section)<0) return 0;
  for(j=i;j<K_COUNT;++j) if(dirty[j] && !seen[j] && same(cfg_specs[j].section,cfg_specs[i].section)) {
   if(fprintf(out,"%s=%s\r\n",cfg_specs[j].key,values[j])<0) return 0;
   seen[j]=1;
  }
 }
 return !ferror(out) && ftell(out)<=CFG_CAPACITY;
}
int cfg_save_changes(const char *path) {
 char temporary[CFG_PATH_SIZE],backup[CFG_PATH_SIZE];
 char *dot,*slash,*backslash,*base; FILE *out; int ok;
#ifdef __WATCOMC__
 unsigned int attributes;
#endif
 if(strlen(path)>=CFG_PATH_SIZE-5) { strcpy(cfg_error,"Configuration path is too long."); return 0; }
 if(!cfg_changed()) return 1;
 if(dirty[K_SHORTCUT_REBOOT] || dirty[K_SHORTCUT_POWEROFF] || dirty[K_SHORTCUT_PAUSE]) {
  int a,b;
  for(a=K_SHORTCUT_REBOOT;a<=K_SHORTCUT_PAUSE;++a) {
   int code=pidos_shortcut_key(values[a]);
   if(code<0) { strcpy(cfg_error,"Choose a supported system shortcut before saving."); return 0; }
   for(b=a+1;code>0 && b<=K_SHORTCUT_PAUSE;++b)
    if(code==pidos_shortcut_key(values[b])) {
     strcpy(cfg_error,"System shortcuts must use different key combinations."); return 0;
    }
  }
 }
 if(cfg_midi()==MIDI_EXTERNAL && !*cfg_get(K_MIDI_PORT)) {
  strcpy(cfg_error,"Choose a connected USB MIDI destination in MIDI settings before saving."); return 0;
 }
 if((dirty[K_RAM] || dirty[K_CPU_TYPE]) &&
   ((same(values[K_CPU_TYPE],"286") && atoi(values[K_RAM])>16) ||
    (one_of(values[K_CPU_TYPE],"8086|80186") && atoi(values[K_RAM])>1))) {
  strcpy(cfg_error,"This CPU cannot address the selected RAM. Reduce RAM or choose a newer CPU."); return 0;
 }
 /* Only pending keys are changed; reload raw bytes to retain unrelated external edits. */
 if(!load_contents(path)) return 0;
 /* A newly created file must not become a legacy CRT file after its first save. */
 if(!file_exists) dirty[K_MONITOR]=1;
#ifdef __WATCOMC__
 if(file_exists && (_dos_getfileattr(path,&attributes) || (attributes&_A_RDONLY))) {
  strcpy(cfg_error,"Configuration is read-only or inaccessible. No changes saved."); return 0;
 }
#endif
    strcpy(temporary, path);
    slash = strrchr(temporary, '/');
    backslash = strrchr(temporary, '\\');
    base = temporary;
    if (slash) base = slash + 1;
    if (backslash && backslash >= base) base = backslash + 1;
    dot = strrchr(base, '.');
    if (dot) *dot = 0;
    strcpy(backup, temporary);
    strcat(temporary, ".TMP");
    strcat(backup, ".BAK");
    if (same(path, temporary) || same(path, backup)) {
        strcpy(cfg_error, "Use a configuration filename other than .TMP or .BAK.");
        return 0;
    }
    /* Refuse to overwrite a recovery file from an interrupted save. */
    out = fopen(temporary, "rb");
    if (out) {
        fclose(out);
        strcpy(cfg_error, "A .TMP recovery file exists. Check it before saving again.");
        return 0;
    }
    out = fopen(temporary, "wb");
    if (!out) {
        strcpy(cfg_error, "Cannot write configuration. Check free space and read-only storage.");
        return 0;
    }
    ok = write_config(out);
    if (fclose(out)) ok = 0;
    if (!ok) {
        remove(temporary);
        strcpy(cfg_error, "Write failed. The original configuration has not been changed.");
        return 0;
    }
    if (file_exists) {
        if (remove(backup) && errno != ENOENT) {
            remove(temporary);
            strcpy(cfg_error, "Cannot replace the previous .BAK backup. Configuration unchanged.");
            return 0;
        }
        if (rename(path, backup)) {
            remove(temporary);
            strcpy(cfg_error, "Cannot back up configuration. The original file is unchanged.");
            return 0;
        }
    }
    if (rename(temporary, path)) {
        if (file_exists && rename(backup, path)) {
            strcpy(cfg_error, "Save failed. Restore the .BAK file; .TMP contains the new settings.");
        } else {
            remove(temporary);
            strcpy(cfg_error, "Save failed. The original configuration has been restored.");
        }
        return 0;
    }
    return cfg_load(path);
}
int cfg_save(const char *path,int profile) {
 if(profile<0 || profile>=PROFILE_COUNT) { strcpy(cfg_error,"Select a CPU profile first."); return 0; }
 if(!cfg_load(path) || !cfg_select_cpu(profile)) return 0;
 /* Explicit profile saves also materialize defaults in sparse/empty files. */
 dirty[K_CPU_TYPE]=dirty[K_CPU_CORE]=dirty[K_CPU_CYCLES]=1;
 return cfg_save_changes(path);
}
