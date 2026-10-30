#include "forms.h"
#include "input_options.h"
#include <string.h>
const char *page_names[PAGE_COUNT] = {"CPU", "Memory", "Video", "Sound", "Mixer", "Input", "Network", "System"};
static const char shortcut_choices[] = PIDOS_SHORTCUT_CHOICES;
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
 } else if(page==PAGE_INPUT) {
  ADD(K_JOY_TYPE,FIELD_CHOICE,"DOS joystick type","2axis=Two 2-axis joysticks (default)|4axis=One 4-axis joystick|4axis_2=4-axis on controller 2|fcs=Thrustmaster FCS|ch=CH Flightstick|none=Disabled|auto=Automatic (stable 2-axis)",0,"Games calibrate and assign DOS joystick controls in their own setup.");
  ADD(K_CONTROLLER_LAYOUT,FIELD_CHOICE,"Controller layout","auto=Automatic (default)|raw=Raw Linux axes/buttons",0,"Automatic normalizes known gamepads; unknown devices use raw input.");
  ADD(K_CONTROLLER_DIRECTION,FIELD_CHOICE,"Directional control","both=Left stick + D-pad (default)|stick=Left stick only|dpad=D-pad only",0,"D-pad controls DOS X/Y axes. Raw devices use their first hat.");
  ADD(K_CONTROLLER_SWAP,FIELD_CHECKBOX,"Swap first two buttons",0,0,"Swap the first two action buttons globally; games can assign actions.");
  ADD(K_JOY_DEADZONE,FIELD_CHOICE,"Stick dead zone","0.0=0 percent|0.1=10 percent|0.15=15 percent|0.2=20 percent|0.25=25 percent (default)|0.3=30 percent|0.4=40 percent|0.5=50 percent",0,"Ignore small stick movements. Selection applies to both controllers.");
  ADD(K_JOY_TIMED,FIELD_CHECKBOX,"Timed joystick axes",0,0,"Try changing this if a game's joystick calibration is unstable.");
  ADD(K_JOY_AUTOFIRE,FIELD_CHECKBOX,"Joystick autofire",0,0,"Repeat fire while an action button is held; normally disabled.");
 } else if(page==PAGE_NETWORK) {
  ADD(K_IPX,FIELD_CHECKBOX,"IPX tunnel",0,0,"IPX multiplayer between emulators over UDP; no NE2000 driver needed.");
  if(!strcmp(cfg_get(K_IPX),"true")) {
   ADD(K_IPX_MODE,FIELD_CHOICE,"IPX startup","manual=Manual IPXNET commands|server=Host a tunnel server|client=Connect to a server",0,"Host one server, then connect every other player to its IP address.");
   if(!strcmp(cfg_get(K_IPX_MODE),"client"))
    ADD(K_IPX_SERVER,FIELD_TEXT,"IPX server address",0,0,"IPv4 address or hostname of the tunnel server.");
   ADD(K_IPX_PORT,FIELD_TEXT,"IPX UDP port",0,0,"Server and clients must use the same UDP port; default 213.");
  }
  ADD(K_NE2000,FIELD_CHECKBOX,"NE2000 Ethernet",0,0,"Emulated network card for DOS TCP/IP tools or a native IPX stack.");
  if(!strcmp(cfg_get(K_NE2000),"true")) {
   ADD(K_NET_BACKEND,FIELD_CHOICE,"Ethernet connection","slirp=SLIRP (TCP/IP with NAT)|pcap=PCAP (wired LAN / native IPX)",0,"SLIRP provides TCP/IP. PCAP connects DOS directly to the wired LAN.");
   if(!strcmp(cfg_get(K_NET_BACKEND),"pcap"))
    ADD(K_NET_INTERFACE,FIELD_TEXT,"Host Ethernet interface",0,0,"Pi wired interface, usually eth0. USB Ethernet may use eth1 or enx...");
   ADD(K_NET_STARTUP,FIELD_CHOICE,"DOS network startup","manual=Manual driver loading|windows=Windows for Workgroups drivers|packet=Packet driver only|dhcp=mTCP with DHCP|static=mTCP with static IPv4|native-ipx=Native IPX (user drivers)",0,"DHCP/static load the bundled packet driver. Native IPX needs ODI files.");
   ADD(-1,FIELD_NET_NIC,"Network card settings...",0,0,"NE2000 I/O port, IRQ and MAC address. Match all external driver settings.");
   ADD(-1,FIELD_NET_TCP,"TCP/IP settings...",0,0,"Static guest IPv4 settings and SLIRP host:guest port forwarding.");
   if(!strcmp(cfg_get(K_NET_STARTUP),"windows"))
    ADD(-1,FIELD_NET_IPX,"Windows network settings...",0,0,"Windows directory for WFWNET. Install NE2000 and protocols inside Windows.");
   if(!strcmp(cfg_get(K_NET_STARTUP),"native-ipx"))
    ADD(-1,FIELD_NET_IPX,"Native IPX settings...",0,0,"Driver directory and Ethernet frame type; see C:\\DOCS\\NETWORK.TXT.");
  }
 } else if(page==PAGE_SYSTEM) {
  ADD(K_AUDIO_OUTPUT,FIELD_CHOICE,"Audio output","hdmi=HDMI (default)|jack=3.5 mm jack (Pi 3/4)|usb=External USB DAC",0,"Physical sound output. Connect USB DAC before boot; reboot to apply.");
  ADD(K_MONITOR,FIELD_CHOICE,"Monitor type","crt=CRT (default)|lcd=LCD",0,"LCD uses a fixed mode. CRT follows DOS resolutions and refresh rates.");
  if(!strcmp(cfg_get(K_MONITOR),"lcd")) {
   ADD(K_LCD_MODE,FIELD_CHOICE,"LCD resolution","640x480|800x600|832x624|1024x768=1024x768 (default)|1280x720|1920x1080",0,"Fixed output resolution from the supplied EDID; reboot to apply.");
   ADD(K_LCD_ASPECT,FIELD_CHOICE,"LCD aspect ratio","4:3=4:3 (default)|original=Original pixels",0,"4:3 corrects DOS geometry. Original pixels preserves the source raster ratio.");
   ADD(K_LCD_LINEAR,FIELD_CHECKBOX,"Linear filtering",0,0,"Smooth LCD scaling with SDL2; off by default. Reboot to apply.");
  }
  ADD(K_KEYBOARD,FIELD_CHOICE,"Keyboard layout",0,"KEYBOARD.LST","Keyboard translation is independent of country and timezone.");
  ADD(K_COUNTRY,FIELD_CHOICE,"DOS country",0,"COUNTRY.LST","Country controls DOS date, number and currency formatting.");
  ADD(K_TIMEZONE,FIELD_CHOICE,"Local timezone",0,"TIMEZONE.LST","Installed IANA timezones; daylight saving changes are automatic.");
  ADD(K_DIAGNOSTICS,FIELD_CHECKBOX,"Diagnostic logs",0,0,"Checked captures diagnostic logs in C:\\SYSTEM after reboot; default off.");
  ADD(K_SHORTCUT_REBOOT,FIELD_CHOICE,"Reboot shortcut",shortcut_choices,0,"Clean reboot. Select None to pass the combination through to DOS.");
  ADD(K_SHORTCUT_POWEROFF,FIELD_CHOICE,"Poweroff shortcut",shortcut_choices,0,"Clean poweroff through the system supervisor; disabled by default.");
  ADD(K_SHORTCUT_PAUSE,FIELD_CHOICE,"Pause shortcut",shortcut_choices,0,"Pause emulation. Repeat this shortcut, or press Escape, to resume.");
  ADD(K_SHORTCUT_SCREENSHOT,FIELD_CHOICE,"Screenshot shortcut",shortcut_choices,0,"Save a PNG in C:\\SYSTEM\\CAPTURES. Requires writable C: storage.");
  ADD(K_SHORTCUT_NEXT_CD,FIELD_CHOICE,"Next CD shortcut",shortcut_choices,0,"Cycle CD images loaded together with IMGMOUNT, without exiting a game.");
  ADD(K_SHORTCUT_NEXT_FLOPPY,FIELD_CHOICE,"Next floppy shortcut",shortcut_choices,0,"Cycle floppy images loaded together with IMGMOUNT in A: or B:.");
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

int form_network_fields(int type, Field *fields) {
 int n=0;
 if(type==FIELD_NET_NIC) {
  ADD(K_NET_BASE,FIELD_CHOICE,"I/O port (hex)","300=300 (default)|200|220|240|260|280|2c0|320|340|360",0,"NE2000 uses 32 ports. Avoid overlaps with sound and MIDI devices.");
  ADD(K_NET_IRQ,FIELD_CHOICE,"IRQ","10=10 (default)|3|4|5|9|11|12|14|15",0,"Use a free IRQ. COM2=3, COM1=4, mouse=12, IDE=14/15.");
  ADD(K_NET_MAC,FIELD_TEXT,"MAC address",0,0,"Use random (default), or a unique unicast address such as AC:DE:48:11:22:33.");
 } else if(type==FIELD_NET_TCP) {
  ADD(K_NET_IP,FIELD_TEXT,"Static DOS IPv4 address",0,0,"Used only for Static startup. SLIRP normally uses 10.0.2.15.");
  ADD(K_NET_MASK,FIELD_TEXT,"Static subnet mask",0,0,"Used only for Static startup. Default: 255.255.255.0.");
  ADD(K_NET_GATEWAY,FIELD_TEXT,"Static gateway",0,0,"Used only for Static startup. SLIRP gateway: 10.0.2.2.");
  ADD(K_NET_DNS,FIELD_TEXT,"Static DNS server",0,0,"Used only for Static startup. SLIRP DNS: 10.0.2.3.");
  ADD(K_NET_TCP_PORTS,FIELD_TEXT,"SLIRP TCP forwards",0,0,"Space-separated host:guest ports, e.g. 8021:21 8080:80. Blank disables.");
  ADD(K_NET_UDP_PORTS,FIELD_TEXT,"SLIRP UDP forwards",0,0,"Space-separated host:guest ports. Used only with SLIRP.");
 } else if(!strcmp(cfg_get(K_NET_STARTUP),"windows")) {
  ADD(K_NET_WINDOWS,FIELD_TEXT,"Windows directory",0,0,"WFWNET loads IFSHLP.SYS and runs NET START here before you run WIN.");
 } else {
  ADD(K_NET_IPX_PATH,FIELD_TEXT,"ODI/IPX driver directory",0,0,"Place LSL.COM, NE2000.COM (ODI) and IPXODI.COM here; not bundled.");
  ADD(K_NET_IPX_FRAME,FIELD_CHOICE,"Ethernet frame type","Ethernet_802.2|Ethernet_802.3|Ethernet_II|Ethernet_SNAP",0,"Must match the other native IPX computers on the wired LAN.");
 }
 return n;
}
