#include "forms.h"
#include <string.h>
const char *page_names[PAGE_COUNT] = {"CPU", "Memory", "Video", "Sound", "Mixer", "System"};
const char *memory_sizes[MEMORY_COUNT] = {"1", "2", "4", "8", "16", "32", "64", "128"};
#define ADD(k,t,l,c,f,h) do { fields[n].key=k; fields[n].type=t; fields[n].label=l; fields[n].choices=c; fields[n].catalog=f; fields[n].help=h; ++n; } while(0)
int form_fields(int page, Field *fields)
{
 int n=0,card=cfg_card(),i;
 static const char *volnames[]={"Master output","Sound Blaster digital","AdLib / FM music","Gravis Ultrasound","Game Blaster / CMS","PC speaker","Tandy three-voice","Tandy DAC","Disney / Covox","Internal MIDI music"};
 if(page==PAGE_VIDEO) {
  ADD(K_MACHINE,FIELD_VIDEO,"Video card",0,0,"Select the emulated video adapter. S3 Trio64 is the default.");
  ADD(K_VOODOO,FIELD_VOODOO,"3dfx Voodoo",0,0,"Checked enables software Voodoo emulation with 12 MiB of video RAM.");
 } else if(page==PAGE_SOUND) {
  ADD(K_SPEAKER,FIELD_CHECKBOX,"PC Speaker",0,0,"Checked enables the PC speaker independently of the sound card.");
  ADD(-1,FIELD_CARD,"Sound card",0,0,"Select the primary emulated card. MIDI remains independent.");
  if(card==CARD_SB2 || card==CARD_PRO2 || card==CARD_SB16 || card==CARD_CMS || card<0) {
   ADD(K_SB_PORT,FIELD_CHOICE,"I/O port (hex)","220|240|260|280|2a0|2c0|2e0",0,"The game must use the same I/O address as this emulated card.");
  }
  if(card==CARD_SB2 || card==CARD_PRO2 || card==CARD_SB16 || card<0) {
   ADD(K_SB_IRQ,FIELD_CHOICE,"IRQ","3|5|7|9|10|11|12",0,"Choose the same interrupt in the game's sound setup.");
   ADD(K_SB_DMA,FIELD_CHOICE,"8-bit DMA","0|1|3",0,"Low DMA channel for 8-bit sampled sound.");
   if(card==CARD_SB16) {
    ADD(K_SB_HDMA,FIELD_CHOICE,"16-bit DMA","5|6|7",0,"High DMA channel for Sound Blaster 16 sampled sound.");
   }
   ADD(K_OPL,FIELD_CHOICE,"FM synthesis","auto=Automatic for card|opl2=OPL2|dualopl2=Dual OPL2|opl3=OPL3|none=Disabled",0,
       "Automatic matches the card; SB Pro 2 and SB16 support OPL3.");
  }
  if(card==CARD_GUS) {
   ADD(K_GUS_PORT,FIELD_CHOICE,"GUS I/O port (hex)","210|220|230|240|250|260|280|2a0|2c0|2e0|300",0,"The game or GUS driver must use this address.");
   ADD(K_GUS_IRQ,FIELD_CHOICE,"GUS IRQ","3|5|7|9|10|11|12",0,"The game or GUS driver must use this interrupt.");
   ADD(K_GUS_DMA,FIELD_CHOICE,"GUS DMA","0|1|3|5|6|7",0,"The game or GUS driver must use this DMA channel.");
   ADD(K_GUS_RAM,FIELD_CHOICE,"GUS sample RAM (KiB)","256|512|768|1024",0,"On-card sample memory for the emulated Ultrasound Classic.");
   ADD(K_GUS_DIR,FIELD_TEXT,"GUS patch directory",0,0,"Some GUS music needs patches in the directory's MIDI subfolder.");
  }
  ADD(K_MIDI_PROFILE,FIELD_MIDI,"MIDI output",0,0,"MIDI music is independent of the sound card used for digital effects.");
  ADD(-1,FIELD_MIDI_SETTINGS,"MIDI settings...",0,0,"MPU-401 interface, port, IRQ and external USB MIDI destination.");
 } else if(page==PAGE_MIXER) {
  for(i=0;i<=K_VOL_MIDI-K_VOL_MASTER;++i) {
   ADD(K_VOL_MASTER+i,FIELD_VOLUME,volnames[i],"0|25|50|75|100|125|150|175|200",0,
       "Left:right percent. 0=mute, 100=normal; above 100 may clip.");
  }
 } else if(page==PAGE_SYSTEM) {
  ADD(K_MONITOR,FIELD_CHOICE,"Monitor type","lcd=LCD (default)|crt=CRT",0,"LCD uses a fixed mode. CRT follows DOS resolutions and refresh rates.");
  if(!strcmp(cfg_get(K_MONITOR),"lcd")) {
   ADD(K_LCD_MODE,FIELD_CHOICE,"LCD resolution","640x480|800x600|832x624|1024x768=1024x768 (default)|1280x720|1920x1080",0,"Fixed output resolution from the supplied EDID; reboot to apply.");
   ADD(K_LCD_ASPECT,FIELD_CHOICE,"LCD aspect ratio","4:3=4:3 (default)|original=Original pixels",0,"4:3 corrects DOS geometry. Original pixels preserves the source raster ratio.");
  }
  ADD(K_KEYBOARD,FIELD_CHOICE,"Keyboard layout",0,"KEYBOARD.LST","Keyboard translation is independent of country and timezone.");
  ADD(K_COUNTRY,FIELD_CHOICE,"DOS country",0,"COUNTRY.LST","Country controls DOS date, number and currency formatting.");
  ADD(K_TIMEZONE,FIELD_CHOICE,"Local timezone",0,"TIMEZONE.LST","Installed IANA timezones; daylight saving changes are automatic.");
  ADD(K_DIAGNOSTICS,FIELD_CHECKBOX,"Diagnostic logs",0,0,"Checked captures diagnostic logs in C:\\SYSTEM after reboot; default off.");
 }
 return n;
}
int form_midi_fields(Field *fields) {
 int n=0;
 ADD(K_MPU,FIELD_CHOICE,"MPU-401 interface","intelligent=Intelligent (default)|uart=UART",0,"Intelligent mode provides broad compatibility, including older Roland games.");
 ADD(K_MPU_BASE,FIELD_CHOICE,"I/O port (hex)","330=330 (default)|300|310|320|332|334|336|340|360|0=Automatic",0,"The game must use the same MIDI interface port, usually 330h.");
 ADD(K_MPU_IRQ,FIELD_CHOICE,"IRQ","-1=Automatic (default)|2|3|5|7|9|10|11|12|15",0,"Automatic normally selects IRQ 9 on AT-class machines; IRQ 2 is redirected.");
 if(cfg_midi()==MIDI_EXTERNAL) {
  ADD(K_MIDI_PORT,FIELD_MIDI_PORT,"USB MIDI destination",0,0,"Select a connected output. Press F5 in the list to rescan devices.");
  ADD(K_MIDI_DELAY,FIELD_CHECKBOX,"Delay SysEx messages",0,0,"Useful for older physical MT-32 modules that can overflow their MIDI buffer.");
 }
 return n;
}
