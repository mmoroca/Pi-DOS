#include <dos.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "midi.h"
char midi_port_ids[MIDI_PORT_LIMIT][23],midi_port_labels[MIDI_PORT_LIMIT][80];
static int count;
int midi_ports(void) { return count; }
int midi_scan(void) {
 char path[40],command[100],line[256],id[24],*label; int handle,i,location; FILE *file;
 count=0;
 for(location=0;location<2;++location) for(i=0;i<32;++i) {
  sprintf(path,location?"C:\\MP%04X.TMP":"C:\\TEMP\\MP%04X.TMP",((unsigned int)_psp+i)&65535U);
  if(!_dos_creatnew(path,0,&handle)) goto reserved;
 }
 return -1;
reserved:
 _dos_close(handle);
 sprintf(command,"Z:\\SYSTEM\\MIXER.COM /LISTMIDI alsa >%s",path);
 if(system(command)==-1) { remove(path); return -1; }
 file=fopen(path,"r");
 if(!file) { remove(path); return -1; }
 while(count<MIDI_PORT_LIMIT && fgets(line,sizeof(line),file)) {
  if(strncmp(line,"@PIDOSMIDI ",11) || sscanf(line+11,"%23s",id)!=1 || strlen(id)!=22 || strncmp(id,"pidos:",6)) continue;
  for(i=6;i<22 && isxdigit((unsigned char)id[i]);++i) {}
  label=strchr(line,'\t'); if(i!=22 || !label) continue;
  ++label; label[strcspn(label,"\r\n")]=0;
  strcpy(midi_port_ids[count],id); strncpy(midi_port_labels[count],label,79); midi_port_labels[count][79]=0;
  ++count;
 }
 fclose(file); remove(path); return count;
}
