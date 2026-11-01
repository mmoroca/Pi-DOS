/* Small, mouse-optional 80x25 DOS setup UI; no extender or FPU required. */
#include <dos.h>
#include <ctype.h>
#include <conio.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "forms.h"
#include "live.h"
#include "midi.h"
#include "pi_dos_version.h"

#define DEFAULT_CONFIG "C:\\SYSTEM\\PIDOS.CFG"
#define NORMAL 0x17
#define PANEL 0x1f
#define DIM 0x1b
#define SELECTED 0x30
#define BUTTON 0x70
#define FOCUSED 0x3f
#define TAB_STEP 9
#define TAB_WIDTH 8

static unsigned short __far saved_screen[2000];
static unsigned short __far setup_screen[2000];
static unsigned short __far *video;
static int screen_dirty;
static unsigned char display_page;
static int mouse_present;
static int monochrome;
static int changed_mode;
static unsigned char old_mode, old_page;
static unsigned short old_cursor, old_shape;

static int equal(const char *a, const char *b)
{
    return stricmp(a, b) == 0;
}

static void mouse_call(unsigned int function, union REGS *r)
{
    r->x.ax = function;
    int86(0x33, r, r);
}

/* Present complete text rows through BIOS, rather than relying on direct
 * guest stores to video RAM to invalidate the host's cached text rendering.
 * AL=2 supplies each character's attribute and keeps the cursor stationary. */
static void screen_present(void)
{
    union REGS r;
    union REGPACK row_regs;
    struct SREGS segments;
    unsigned short cursor, last;
    int row;
    if (!screen_dirty) return;
    r.h.ah = 3; r.h.bh = display_page;
    int86(0x10, &r, &r);
    cursor = r.x.dx;
    for (row = 0; row < 25; ++row) {
        segread(&segments);
        memset(&row_regs, 0, sizeof(row_regs));
        row_regs.w.ds = segments.ds;
        row_regs.w.es = FP_SEG(setup_screen);
        row_regs.w.ax = 0x1302;
        row_regs.w.bx = (unsigned short)display_page << 8;
        /* A teletype write of the bottom-right cell scrolls the screen.
         * Write that cell separately with the non-advancing BIOS service. */
        row_regs.w.cx = row == 24 ? 79 : 80;
        row_regs.w.dx = (unsigned short)row << 8;
        row_regs.w.bp = FP_OFF(setup_screen) + row * 160U;
        intr(0x10, &row_regs);
    }
    r.x.ax = 0x0200; r.h.bh = display_page; r.x.dx = 0x184f;
    int86(0x10, &r, &r);
    last = setup_screen[1999];
    r.h.ah = 9; r.h.al = (unsigned char)last;
    r.h.bh = display_page; r.h.bl = (unsigned char)(last >> 8); r.x.cx = 1;
    int86(0x10, &r, &r);
    r.x.ax = 0x0200; r.h.bh = display_page; r.x.dx = cursor;
    int86(0x10, &r, &r);
    screen_dirty = 0;
}

static void mouse_visible(int visible)
{
    union REGS r;
    if (visible) screen_present();
    if (mouse_present) mouse_call(visible ? 1 : 2, &r);
}

static void screen_start(void)
{
    union REGS r;
    volatile unsigned short __far *original;
    unsigned char __far *mouse_handler;
    unsigned char columns, rows;
    int i;
    r.h.ah = 0x0f;
    int86(0x10, &r, &r);
    old_mode = r.h.al & 0x7f;
    columns = r.h.ah;
    old_page = r.h.bh;
    rows = *(unsigned char __far *)MK_FP(0x40, 0x84);
    r.h.ah = 3;
    r.h.bh = old_page;
    int86(0x10, &r, &r);
    old_cursor = r.x.dx;
    old_shape = r.x.cx;
    /* Hercules may initially report mode 3; the equipment word identifies
     * its monochrome adapter before we explicitly enter mode 7. */
    int86(0x11, &r, &r);
    monochrome = old_mode == 7 || (r.x.ax & 0x30) == 0x30;
    original = (volatile unsigned short __far *)MK_FP(monochrome ? 0xb000 : 0xb800,
                                            old_page * 4096U);
    for (i = 0; i < 2000; ++i) setup_screen[i] = saved_screen[i] = original[i];
    /* Boot already supplies 80x25 text. Re-entering mode 3 here needlessly
     * restarts VGA drawing and the KMS surface before the welcome is visible.
     * Keep its font, timings and active page; change mode only when required.
     * Older CGA/MDA BIOSes leave the EGA row-count byte zero. */
    changed_mode = old_mode != (monochrome ? 7 : 3) || columns != 80 ||
                   (rows != 0 && rows != 24);
    if (monochrome) old_mode = 7;
    if (changed_mode) {
        r.x.ax = monochrome ? 7 : 3;
        int86(0x10, &r, &r);
    }
    display_page = changed_mode ? 0 : old_page;
    video = setup_screen;
    screen_dirty = 0;
    r.h.ah = 1;
    r.x.cx = 0x2000;
    int86(0x10, &r, &r);
    mouse_handler = (unsigned char __far *)_dos_getvect(0x33);
    mouse_present = 0;
    if ((FP_SEG(mouse_handler) || FP_OFF(mouse_handler)) && *mouse_handler != 0xcf) {
        mouse_call(0, &r);
        mouse_present = r.x.ax == 0xffff;
    }
    if (mouse_present) {
        /* Text-mode coordinates are eight mouse units per character cell. */
        r.x.cx = 0; r.x.dx = 639; mouse_call(7, &r);
        r.x.cx = 0; r.x.dx = 199; mouse_call(8, &r);
        r.x.bx = 0; r.x.cx = 0x77ff; r.x.dx = 0x7700; mouse_call(10, &r);
        mouse_visible(1);
    }
}

static void screen_end(void)
{
    union REGS r;
    volatile unsigned short __far *original;
    int i;
    mouse_visible(0);
    if (changed_mode) {
        r.x.ax = old_mode;
        int86(0x10, &r, &r);
    }
    original = (volatile unsigned short __far *)MK_FP(old_mode == 7 ? 0xb000 : 0xb800,
                                            old_page * 4096U);
    if (old_mode == 3 || old_mode == 7)
        for (i = 0; i < 2000; ++i) original[i] = saved_screen[i];
    r.h.ah = 5; r.h.al = old_page;
    int86(0x10, &r, &r);
    r.h.ah = 1; r.x.cx = old_shape;
    int86(0x10, &r, &r);
    r.h.ah = 2; r.h.bh = old_page; r.x.dx = old_cursor;
    int86(0x10, &r, &r);
}

static unsigned char display_color(unsigned char color)
{
    if (!monochrome) return color;
    if (color == SELECTED || color == FOCUSED) return 0x70;
    return color == PANEL ? 0x0f : 0x07;
}

static void fill(int x, int y, int width, int height, unsigned char color)
{
    int row, col;
    screen_dirty = 1;
    color = display_color(color);
    for (row = y; row < y + height; ++row)
        for (col = x; col < x + width; ++col)
            video[row * 80 + col] = ((unsigned int)color << 8) | ' ';
}

static void text(int x, int y, const char *s, unsigned char color, int width)
{
    int n;
    screen_dirty = 1;
    color = display_color(color);
    for (n = 0; n < width && *s; ++n)
        video[y * 80 + x + n] = ((unsigned int)color << 8) | (unsigned char)*s++;
}

static void box(int x, int y, int width, int height)
{
    int i;
    fill(x, y, width, height, NORMAL);
    for (i = 1; i < width - 1; ++i) {
        video[y*80 + x+i] = (PANEL << 8) | 196;
        video[(y+height-1)*80 + x+i] = (PANEL << 8) | 196;
    }
    for (i = 1; i < height - 1; ++i) {
        video[(y+i)*80 + x] = (PANEL << 8) | 179;
        video[(y+i)*80 + x+width-1] = (PANEL << 8) | 179;
    }
    video[y*80+x] = (PANEL << 8) | 218;
    video[y*80+x+width-1] = (PANEL << 8) | 191;
    video[(y+height-1)*80+x] = (PANEL << 8) | 192;
    video[(y+height-1)*80+x+width-1] = (PANEL << 8) | 217;
}

static unsigned int key_read(int blocking)
{
    union REGS r;
    if (!blocking && !kbhit()) return 0;
    r.h.ah = 0;
    int86(0x16, &r, &r);
    return r.x.ax;
}

static void message(const char *title, const char *body)
{
    union REGS r;
    unsigned int buttons = 0;
    unsigned int split = (unsigned int)strlen(body);
    char first[59];
    mouse_visible(0);
    fill(9, 9, 64, 7, 0x00);
    box(8, 8, 64, 7);
    text(11, 9, title, PANEL, 58);
    if (split > 58) {
        split = 58;
        while (split && body[split] != ' ') --split;
        if (!split) split = 58;
    }
    memcpy(first, body, split);
    first[split] = 0;
    text(11, 11, first, PANEL, 58);
    while (body[split] == ' ') ++split;
    text(11, 12, body + split, PANEL, 58);
    text(11, 13, "Press any key or click to continue.", DIM, 58);
    mouse_visible(1);
    /* Require a new click, rather than the Save button's held mouse button. */
    if (mouse_present) {
        do { mouse_call(3, &r); } while (r.x.bx & 3);
    }
    while (!key_read(0)) {
        if (mouse_present) {
            mouse_call(3, &r);
            buttons = r.x.bx & 3;
        }
        if (buttons) break;
        /* BIOS idle interrupt; avoid a hot busy loop when no input arrives. */
        int86(0x28, &r, &r);
    }
}


static char catalog_dir[CFG_PATH_SIZE]="Z:\\BIN\\";
static char choice_values[10][CFG_VALUE_SIZE],choice_labels[10][CFG_VALUE_SIZE];
static int choice_custom[10];
static int contains(const char *text,const char *search) {
 unsigned int i,j;
 for(i=0;text[i];++i) {
  for(j=0;search[j] && text[i+j] && tolower((unsigned char)text[i+j])==tolower((unsigned char)search[j]);++j) {}
  if(!search[j]) return 1;
 }
 return !*search;
}
static void split_choice(char *line,char *value,char *label) {
 char *separator=strchr(line,'\t');
 if(!separator) separator=strchr(line,'=');
 if(separator) { *separator++=0; strncpy(label,separator,CFG_VALUE_SIZE-1); }
 else strncpy(label,line,CFG_VALUE_SIZE-1);
 label[CFG_VALUE_SIZE-1]=0;
 strncpy(value,line,CFG_VALUE_SIZE-1); value[CFG_VALUE_SIZE-1]=0;
}
static FILE *catalog_open(const char *name) {
 char path[CFG_PATH_SIZE]; FILE *file;
 if(strlen(catalog_dir)+strlen(name)>=sizeof(path)) return NULL;
 strcpy(path,catalog_dir); strcat(path,name); file=fopen(path,"r");
 if(file) return file;
 strcpy(path,"Z:\\BIN\\"); strcat(path,name); return fopen(path,"r");
}
/* Walk catalog files on demand rather than keeping hundreds of strings in DOS RAM. */
static int choices(const Field *field,const char *search,int offset) {
 FILE *file=NULL; const char *p=field->choices; char line[180],value[CFG_VALUE_SIZE],label[CFG_VALUE_SIZE];
 int total=0,index=0,available,custom;
 if(field->catalog) { file=catalog_open(field->catalog); if(!file) return -1; }
 for(;;) {
  available=0;
  if(field->type==FIELD_CARD) {
   if(index<CARD_COUNT) { sprintf(line,"%d=%s",index,card_names[index]); ++index; available=1; }
  } else if(field->type==FIELD_VIDEO) {
   if(index<VIDEO_PROFILE_COUNT) {
    sprintf(line,"%s=%s%s",video_profiles[index].id,video_profiles[index].label,index==DEFAULT_VIDEO_PROFILE?" (default)":"");
    ++index; available=1;
   }
  } else if(field->type==FIELD_MIDI) {
   if(index<MIDI_PROFILE_COUNT) {
    sprintf(line,"%s=%s%s",midi_profiles[index].id,index?midi_profiles[index].label:"None",index==DEFAULT_MIDI_PROFILE?" (default)":"");
    ++index; available=1;
   }
  } else if(field->type==FIELD_MIDI_PORT) {
   if(index<midi_ports()) {
    sprintf(line,"%s=%s",midi_port_ids[index],midi_port_labels[index]); ++index; available=1;
   }
  } else if(file) {
   if(fgets(line,sizeof(line),file)) { line[strcspn(line,"\r\n")]=0; available=1; }
  } else if(p && *p) {
   unsigned int n=0;
   while(*p && *p!='|') { if(n<sizeof(line)-1) line[n++]=*p; ++p; }
   if(*p) ++p; line[n]=0; available=1;
  }
  if(!available) break;
  split_choice(line,value,label);
  if(!contains(value,search) && !contains(label,search)) continue;
  if(total>=offset && total<offset+10) {
   strcpy(choice_values[total-offset],value); strcpy(choice_labels[total-offset],label); choice_custom[total-offset]=0;
  }
  ++total;
 }
 if(file) fclose(file);
 custom=field->type==FIELD_VOLUME || field->catalog;
 if(custom && !*search) {
  if(total>=offset && total<offset+10) {
   choice_values[total-offset][0]=0; strcpy(choice_labels[total-offset],"Custom value..."); choice_custom[total-offset]=1;
  }
  ++total;
 }
 return total;
}
static int catalog_has(const char *name,const char *value) {
 FILE *file=catalog_open(name); char line[180],key[CFG_VALUE_SIZE],label[CFG_VALUE_SIZE];
 if(!file) return 0;
 while(fgets(line,sizeof(line),file)) {
  line[strcspn(line,"\r\n")]=0; split_choice(line,key,label);
  if(equal(value,key)) { fclose(file); return 1; }
 }
 fclose(file); return 0;
}
static int catalog_find(const char *name,const char *value,char *label,int *total) {
 FILE *file=catalog_open(name); char line[180],key[CFG_VALUE_SIZE],title[CFG_VALUE_SIZE];
 int found=-1;
 *total=0; if(!file) return -1;
 while(fgets(line,sizeof(line),file)) {
  line[strcspn(line,"\r\n")]=0; split_choice(line,key,title);
  if(equal(value,key)) { found=*total; if(label) strcpy(label,title); }
  ++*total;
 }
 fclose(file); return found;
}
static void field_value(const Field *field,char *result) {
 int card,offset,total,j; const char *value;
 if(field->type==FIELD_CARD) {
  card=cfg_card(); strcpy(result,card<0?"Custom / multiple devices":card_names[card]); return;
 }
 if(field->type==FIELD_NET_NIC || field->type==FIELD_NET_TCP || field->type==FIELD_NET_IPX) { strcpy(result,"Configure..."); return; }
 if(field->type==FIELD_MIDI_SETTINGS) { strcpy(result,"[ Open ]"); return; }
 if(field->type==FIELD_MIDI) {
  int profile=cfg_midi();
  if(profile<0) strcpy(result,"Custom MIDI settings");
  else sprintf(result,"%s%s",profile?midi_profiles[profile].label:"None",profile==DEFAULT_MIDI_PROFILE?" (default)":"");
  return;
 }
 value=cfg_get(field->key);
 if(field->type==FIELD_MIDI_PORT) {
  int i;
  strcpy(result,*value?"Unavailable / saved device":"Not selected");
  for(i=0;i<midi_ports();++i) if(equal(value,midi_port_ids[i])) { strcpy(result,midi_port_labels[i]); break; }
  return;
 }
 if(field->type==FIELD_VIDEO && video_profile_find(value)>=0) {
  int profile=video_profile_find(value);
  sprintf(result,"%s%s",video_profiles[profile].label,profile==DEFAULT_VIDEO_PROFILE?" (default)":""); return;
 }
 if(field->key==K_RAM && strcmp(cfg_get(K_RAM_KB),"0")) {
  sprintf(result,"%.12s MiB + %.12s KiB",value=cfg_get(K_RAM),cfg_get(K_RAM_KB)); return;
 }
 if(field->type==FIELD_CHECKBOX) { strcpy(result,equal(value,"true")?"[X]":"[ ]"); return; }
 if(field->type==FIELD_VOODOO) { strcpy(result,equal(value,"false")?"[ ]":"[X]"); return; }
 strncpy(result,value,CFG_VALUE_SIZE-1); result[CFG_VALUE_SIZE-1]=0;
 if(field->type==FIELD_VOLUME || field->type==FIELD_TEXT) return;
 if(field->catalog) {
  static char cached_values[2][CFG_VALUE_SIZE],cached_labels[2][CFG_VALUE_SIZE];
  int slot=field->key==K_KEYBOARD?0:1;
  if(field->key==K_TIMEZONE) return;
  if(strcmp(cached_values[slot],value)) {
   strcpy(cached_values[slot],value); strcpy(cached_labels[slot],value);
   catalog_find(field->catalog,value,cached_labels[slot],&total);
  }
  strcpy(result,cached_labels[slot]); return;
 }
 for(offset=0;;offset+=10) {
  total=choices(field,"",offset); if(total<0) return;
  for(j=0;j<10 && offset+j<total;++j) if(equal(choice_values[j],value)) { strcpy(result,choice_labels[j]); return; }
  if(offset+10>=total) return;
 }
}
static int input_dialog_mode(const char *title,char *value,int typing_test) {
 unsigned int key,ch,scan; int cursor=(int)strlen(value),selected=1,redraw=1,x,y,previous=1;
 union REGS r;
 for(;;) {
  if(redraw) {
   int start=cursor>52?cursor-52:0;
   mouse_visible(0); box(7,7,66,9); text(10,8,title,PANEL,59);
   fill(10,10,59,1,SELECTED); text(11,10,value+start,SELECTED,56);
   text(11+cursor-start,10,"_",FOCUSED,1);
   text(10,12,typing_test?"Try letters and symbols. Ctrl+U clears; Enter/Esc returns.":
    "Type a value. Ctrl+U clears; Enter accepts; Esc cancels.",DIM,59);
   fill(33,14,12,1,BUTTON); text(33,14,"    OK      ",BUTTON,12);
   fill(49,14,17,1,BUTTON); text(49,14,"     Cancel      ",BUTTON,17);
   mouse_visible(1); redraw=0;
  }
  key=key_read(0); ch=key&255; scan=key>>8;
  if(ch==27) return 0;
  if(ch==13) return 1;
  if(key) {
   int length=(int)strlen(value); redraw=1;
   if(ch==21) { value[0]=0; cursor=0; selected=0; }
   else if(scan==75) { if(cursor) --cursor; selected=0; }
   else if(scan==77) { if(cursor<length) ++cursor; selected=0; }
   else if(scan==71) { cursor=0; selected=0; }
   else if(scan==79) { cursor=length; selected=0; }
   else if(ch==8) {
    if(selected) { value[0]=0; cursor=0; }
    else if(cursor) { memmove(value+cursor-1,value+cursor,length-cursor+1); --cursor; }
    selected=0;
   } else if(scan==83 && !ch && cursor<length) { memmove(value+cursor,value+cursor+1,length-cursor); selected=0; }
   else if(ch>=32 && (ch<127 || (typing_test && ch>127))) {
    if(selected) { value[0]=0; cursor=0; length=0; selected=0; }
    if(length<CFG_VALUE_SIZE-1) { memmove(value+cursor+1,value+cursor,length-cursor+1); value[cursor++]=(char)ch; }
   }
  }
  if(mouse_present) {
   mouse_call(3,&r); x=r.x.cx/8; y=r.x.dx/8;
   if((r.x.bx&1) && !(previous&1) && y==14) {
    if(x>=33 && x<45) return 1;
    if(x>=49 && x<66) return 0;
   }
   previous=r.x.bx;
  }
  if(!key) int86(0x28,&r,&r);
 }
}
static int input_dialog(const char *title,char *value) {
 return input_dialog_mode(title,value,0);
}
static int picker(const Field *field,char *result) {
 char search[32]="",current[CFG_VALUE_SIZE],line[80];
 int selected=-1,offset=0,total,j,redraw=1,x,y,previous=1,activate=0;
 unsigned int key,ch,scan; union REGS r;
 if(field->type==FIELD_MIDI_PORT) midi_scan();
 if(field->type==FIELD_CARD) sprintf(current,"%d",cfg_card());
 else { strncpy(current,cfg_get(field->key),sizeof(current)); current[sizeof(current)-1]=0; }
 if(field->type==FIELD_VIDEO && video_profile_find(current)>=0)
  strcpy(current,video_profiles[video_profile_find(current)].id);
 if(field->type==FIELD_VOLUME) {
  int left,right; if(sscanf(current,"%d:%d",&left,&right)==2 && left==right) sprintf(current,"%d",left);
 }
 if(field->catalog) { selected=catalog_find(field->catalog,current,NULL,&total); ++total; }
 else for(offset=0;;offset+=10) {
  total=choices(field,"",offset);
  if(total<0) { message("Selection list unavailable","The image needs the .LST catalogs beside PIDOSCFG.EXE in Z:\\BIN."); return 0; }
  for(j=0;j<10 && offset+j<total;++j) if(equal(choice_values[j],current)) selected=offset+j;
  if(offset+10>=total) break;
 }
 if(selected<0) selected=(field->type==FIELD_VOLUME || field->catalog)?total-1:0;
 offset=(selected/10)*10;
 for(;;) {
  if(redraw) {
   total=choices(field,search,offset);
   mouse_visible(0); box(5,3,70,18); text(8,3,field->label,PANEL,60);
   sprintf(line,"Search: %.31s",search); text(8,4,line,PANEL,62);
   for(j=0;j<10 && offset+j<total;++j) {
    unsigned char color=offset+j==selected?SELECTED:NORMAL;
    fill(7,6+j,66,1,color); text(9,6+j,choice_labels[j],color,62);
   }
   if(!total && field->type==FIELD_MIDI_PORT) text(9,6,"No MIDI outputs found. Connect a device and rescan.",PANEL,62);
   sprintf(line,"%d of %d   Type to search; arrows/PgUp/PgDn; Enter selects.",total?selected+1:0,total);
   text(8,17,line,DIM,64);
   if(field->type==FIELD_MIDI_PORT) text(8,18,"F5: rescan devices (or click this line)",DIM,60);
   fill(8,19,17,1,BUTTON); text(8,19,"   Previous page ",BUTTON,17);
   fill(29,19,17,1,BUTTON); text(29,19,"    Next page    ",BUTTON,17);
   fill(55,19,15,1,BUTTON); text(55,19," Cancel (Esc)  ",BUTTON,15);
   mouse_visible(1); redraw=0;
  }
  key=key_read(0); ch=key&255; scan=key>>8; activate=0;
  if(ch==27) return 0;
  if(ch==13 && total) activate=1;
  if(key && !activate) {
   redraw=1;
   if(scan==63 && field->type==FIELD_MIDI_PORT) { midi_scan(); selected=0; search[0]=0; }
   else if(scan==72 && total) selected=selected>0?selected-1:total-1;
   else if(scan==80 && total) selected=(selected+1)%total;
   else if(scan==71) selected=0;
   else if(scan==79 && total) selected=total-1;
   else if(scan==73) { selected-=10; if(selected<0) selected=0; }
   else if(scan==81 && total) { selected+=10; if(selected>=total) selected=total-1; }
   else if(ch==8) { if(*search) search[strlen(search)-1]=0; selected=0; }
   else if(ch>=32 && ch<127 && strlen(search)<sizeof(search)-1) {
    j=(int)strlen(search); search[j]=(char)ch; search[j+1]=0; selected=0;
   }
   offset=(selected/10)*10;
  }
  if(mouse_present) {
   mouse_call(3,&r); x=r.x.cx/8; y=r.x.dx/8;
   if((r.x.bx&1) && !(previous&1)) {
    if(y==18 && x>=8 && x<68 && field->type==FIELD_MIDI_PORT) { midi_scan(); selected=0; search[0]=0; redraw=1; }
    else if(x>=7 && x<73 && y>=6 && y<16 && offset+y-6<total) { selected=offset+y-6; activate=1; }
    else if(y==19 && x>=8 && x<25) { selected=offset>0?offset-10:0; redraw=1; }
    else if(y==19 && x>=29 && x<46 && total) { selected=offset+10<total?offset+10:total-1; redraw=1; }
    else if(y==19 && x>=55 && x<70) return 0;
    offset=(selected/10)*10;
   }
   previous=r.x.bx;
  }
  if(activate) {
   choices(field,search,offset); j=selected-offset;
   if(choice_custom[j]) {
    strcpy(result,cfg_get(field->key));
    return input_dialog(field->label,result);
   }
   strcpy(result,choice_values[j]); return 1;
  }
  if(!key) int86(0x28,&r,&r);
 }
}
static void midi_settings(void);
static void network_settings(int type);
static void edit_field(const Field *field) {
 char value[CFG_VALUE_SIZE]; int card;
 if(field->type==FIELD_NET_NIC || field->type==FIELD_NET_TCP || field->type==FIELD_NET_IPX) { network_settings(field->type); return; }
 if(field->type==FIELD_MIDI_SETTINGS) { midi_settings(); return; }
 if(field->type==FIELD_CHECKBOX) { cfg_set(field->key,equal(cfg_get(field->key),"true")?"false":"true"); return; }
 if(field->type==FIELD_VOODOO) { cfg_toggle_voodoo(); return; }
 if(field->type==FIELD_TEXT) {
  strcpy(value,cfg_get(field->key)); if(!input_dialog(field->label,value)) return;
 } else if(!picker(field,value)) return;
 if(field->type==FIELD_CARD) {
  card=atoi(value); if(card!=cfg_card()) cfg_select_card(card); return;
 }
 if(field->type==FIELD_MIDI) { cfg_select_midi(midi_profile_find(value)); return; }
 if(field->key==K_TIMEZONE && !catalog_has("TIMEZONE.LST",value)) {
  message("Unknown timezone","Choose an installed timezone from the list. Type a region or city to search."); return;
 }
 if(!cfg_set(field->key,value)) message("Invalid setting",cfg_error);
 else if(field->key==K_RAM) cfg_set(K_RAM_KB,"0");
 else if(field->key==K_MIDI_PORT && cfg_midi()==MIDI_EXTERNAL) cfg_set(K_MIDI_CONFIG,value);
}
static void settings_dialog(const char *title, Field *fields, int n) {
 char value[CFG_VALUE_SIZE];
 int focus=0,redraw=1,x,y,previous=1; unsigned int key,ch,scan; union REGS r;
 for(;;) {
  if(redraw) {
   int i;
   mouse_visible(0); box(5,5,70,15); text(8,5,title,PANEL,60);
   for(i=0;i<n;++i) {
    unsigned char color=focus==i?SELECTED:NORMAL;
    fill(7,8+i,66,1,color); field_value(&fields[i],value);
    text(9,8+i,fields[i].label,color,26); text(37,8+i,value,color,34);
   }
   text(8,15,"Changes stay pending until Save on the main screen.",DIM,62);
   fill(49,17,20,1,focus==n?FOCUSED:BUTTON); text(49,17,"    Back (Esc)      ",focus==n?FOCUSED:BUTTON,20);
   text(8,18,focus<n?fields[focus].help:"Enter or Esc returns to the main page.",PANEL,63);
   mouse_visible(1); redraw=0;
  }
  key=key_read(0); ch=key&255; scan=key>>8;
  if(ch==27) return;
  if(key) {
   redraw=1;
   if(ch==9 || scan==80) focus=(focus+1)%(n+1);
   else if(scan==72 || (scan==15 && !ch)) focus=focus?focus-1:n;
   else if(ch==13 || ch==' ') { if(focus==n) return; edit_field(&fields[focus]); }
  }
  if(mouse_present) {
   mouse_call(3,&r); x=r.x.cx/8; y=r.x.dx/8;
   if((r.x.bx&1) && !(previous&1)) {
    if(y==17 && x>=49 && x<69) return;
    if(x>=7 && x<73 && y>=8 && y<8+n) { focus=y-8; edit_field(&fields[focus]); redraw=1; }
   }
   previous=r.x.bx;
  }
  if(!key) int86(0x28,&r,&r);
 }
}
static void midi_settings(void) {
 Field fields[5];
 if(cfg_midi()<=MIDI_NONE) { message("MIDI settings","Select a MIDI output profile first."); return; }
 if(cfg_midi()==MIDI_EXTERNAL) midi_scan();
 settings_dialog(" MIDI settings ",fields,form_midi_fields(fields));
}
static void network_settings(int type) {
 Field fields[6];
 settings_dialog(type==FIELD_NET_NIC?" Network card settings ":
  type==FIELD_NET_TCP?" TCP/IP settings ":
  !strcmp(cfg_get(K_NET_STARTUP),"windows")?" Windows network settings ":" Native IPX settings ",fields,form_network_fields(type,fields));
}
static void step_volume(const Field *field,int direction) {
 int left=100,right; char value[40];
 if(sscanf(cfg_get(field->key),"%d:%d",&left,&right)!=2) right=left;
 left+=direction*5; right+=direction*5;
 if(left<0) left=0; if(left>200) left=200; if(right<0) right=0; if(right>200) right=200;
 sprintf(value,"%d:%d",left,right); cfg_set(field->key,value);
}
static int memory_selected(void) {
 int i;
 if(strcmp(cfg_get(K_RAM_KB),"0")) return -1;
 for(i=0;i<MEMORY_COUNT;++i) if(!strcmp(cfg_get(K_RAM),memory_sizes[i])) return i;
 return -1;
}
static void select_memory(int selected) {
 if(selected>=0 && selected<MEMORY_COUNT) {
  cfg_set(K_RAM,memory_sizes[selected]); cfg_set(K_RAM_KB,"0");
 }
}
static int field_offset(int n,int focus) {
 if(focus>=n) focus=n-1;
 return focus>=FORM_VISIBLE_FIELDS?focus-FORM_VISIBLE_FIELDS+1:0;
}
static void draw_page(int page,int focus,const char *path) {
 Field fields[FORM_MAX_FIELDS]; int i,n=(page==PAGE_CPU || page==PAGE_MEMORY)?1:form_fields(page,fields);
 int offset=field_offset(n,focus),row;
 int selected=page==PAGE_MEMORY?memory_selected():cfg_profile(); char value[CFG_VALUE_SIZE],line[160];
 mouse_visible(0); fill(0,0,80,25,NORMAL); fill(0,0,80,1,SELECTED);
 text(3,0,"Pi-DOS " PI_DOS_VERSION " Setup",SELECTED,40); text(56,0,cfg_changed()?"Unsaved changes":"Configuration loaded",SELECTED,23);
 for(i=0;i<PAGE_COUNT;++i) {
  unsigned char color=i==page?FOCUSED:BUTTON;
  fill(4+i*TAB_STEP,2,TAB_WIDTH,1,color); text(5+i*TAB_STEP,2,page_names[i],color,7);
 }
 box(3,4,74,13); text(6,4,page==PAGE_CPU?" CPU profile ":page_names[page],PANEL,30);
 if(page==PAGE_CPU) {
  for(i=0;i<PROFILE_COUNT;++i) {
   unsigned char color=i==selected?(focus==0?SELECTED:BUTTON):NORMAL;
   fill(5,5+i,70,1,color); text(7,5+i,i==selected?"(*)":"( )",color,3);
   sprintf(value,"%s%s%s",cpu_profiles[i].label,cpu_profile_recommendation(i),
           i==cfg_cpu_default()?" [recommended]":"");
   text(12,5+i,value,color,58);
  }
  text(4,18,selected<0?"Custom CPU settings; select a profile to replace them.":cpu_profiles[selected].description,PANEL,72);
  sprintf(line,"CPU model: %.22s   Cycles: %.22s",cfg_type,cfg_cycles); text(4,19,line,DIM,72);
 } else if(page==PAGE_MEMORY) {
  for(i=0;i<MEMORY_COUNT;++i) {
   unsigned char color=i==selected?(focus==0?SELECTED:BUTTON):NORMAL;
   fill(5,5+i,70,1,color); text(7,5+i,i==selected?"(*)":"( )",color,3);
   sprintf(value,"%s MiB%s",memory_sizes[i],!strcmp(memory_sizes[i],cfg_specs[K_RAM].default_value)?" (default)":"");
   text(12,5+i,value,color,58);
  }
  if(selected<0) {
   sprintf(line,"Custom RAM: %.12s MiB + %.12s KiB. Choose a size to replace it.",cfg_get(K_RAM),cfg_get(K_RAM_KB));
   text(4,18,line,PANEL,72);
  } else text(4,18,"Above 16 MiB can break older software. Use 16 unless an app needs more.",PANEL,72);
  text(4,19,"Some older programs need less RAM; CPU type can also limit usable RAM.",DIM,72);
 } else {
  if(offset>0) text(65,4,"\x18 More",PANEL,8);
  if(offset+FORM_VISIBLE_FIELDS<n) text(65,16,"\x19 More",PANEL,8);
  for(i=offset;i<n && i<offset+FORM_VISIBLE_FIELDS;++i) {
   unsigned char color=focus==i?SELECTED:NORMAL;
   row=5+i-offset;
   field_value(&fields[i],value); fill(5,row,70,1,color); text(7,row,fields[i].label,color,28);
   if(fields[i].type==FIELD_VOLUME) {
    text(38,row,"[-]",color,3); text(44,row,value,color,20); text(70,row,"[+]",color,3);
   } else text(37,row,value,color,37);
  }
  if(focus<n) text(4,18,fields[focus].help,PANEL,72);
  if(page==PAGE_MIXER) text(4,19,live_status(),DIM,72);
  if(page==PAGE_NETWORK) text(4,19,"Wired Ethernet. NETUP retries startup; NETINFO shows IPX tunnel status.",DIM,72);
  if(page==PAGE_INPUT) text(4,19,"Controllers reconnect automatically. Game calibration may need restarting.",DIM,72);
  if(page==PAGE_VIDEO) text(4,19,"Voodoo uses software rendering; Glide passthrough stays disabled.",DIM,72);
  if(page==PAGE_SOUND) {
   int card=cfg_card();
   text(4,19,card==CARD_GUS?"GUS music may need patch files in C:\\ULTRASND\\MIDI.":
    card==CARD_ADLIB?"AdLib uses OPL2 at 388h; this FM port is fixed.":
    card==CARD_TANDY?"Tandy ports and DMA are fixed by the emulated hardware.":
    "Game/driver port, IRQ and DMA must match this card's settings.",DIM,72);
  }
 }
 text(4,20,page==PAGE_MIXER?"Left/right: +/-5%. Save keeps levels. Restore defaults: this page only.":
  "Save all pages, then REBOOT. Restore defaults resets this page only.",PANEL,72);
 fill(4,22,28,1,focus==n?FOCUSED:BUTTON); text(4,22,"   Restore defaults (F3)    ",focus==n?FOCUSED:BUTTON,28);
 fill(44,22,14,1,focus==n+1?FOCUSED:BUTTON); text(44,22,"  Save (F2)   ",focus==n+1?FOCUSED:BUTTON,14);
 fill(60,22,15,1,focus==n+2?FOCUSED:BUTTON); text(60,22," Cancel (Esc)  ",focus==n+2?FOCUSED:BUTTON,15);
 text(4,23,mouse_present?"Keyboard + mouse":"Keyboard (no mouse driver)",DIM,36);
 fill(0,24,80,1,SELECTED); text(1,24,"F5/F6: pages  Tab: focus  Enter: edit",SELECTED,43); text(45,24,path,SELECTED,34);
 mouse_visible(1);
}
static int setup(const char *path) {
 int page=PAGE_CPU,focus=0,redraw=1,n,selected,count,action,x,y,previous=0; unsigned int key,ch,scan;
 Field fields[FORM_MAX_FIELDS]; union REGS r;
 screen_start();
 for(;;) {
  n=(page==PAGE_CPU || page==PAGE_MEMORY)?1:form_fields(page,fields);
  if(focus>=n+3) focus=0;
  if(redraw) { mouse_visible(0); live_update(page==PAGE_MIXER); mouse_visible(1); draw_page(page,focus,path); redraw=0; }
  key=key_read(0); ch=key&255; scan=key>>8; action=0;
  if(key) {
   redraw=1;
   if(ch==27 || (!ch && (scan==45 || scan==46))) action=2;
   else if(scan==60 || (!ch && scan==31)) action=1;
   else if(scan==61 || (!ch && scan==19)) action=3;
   else if(scan==63 || scan==64) { page=(page+(scan==64?1:PAGE_COUNT-1))%PAGE_COUNT; focus=0; continue; }
   else if(ch==9) focus=(focus+1)%(n+3);
   else if(scan==15 && !ch) focus=(focus+n+2)%(n+3);
   else if((page==PAGE_CPU || page==PAGE_MEMORY) && (scan==72 || scan==80 || scan==71 || scan==79)) {
    focus=0; selected=page==PAGE_CPU?cfg_profile():memory_selected();
    count=page==PAGE_CPU?PROFILE_COUNT:MEMORY_COUNT;
    if(scan==71) selected=0; else if(scan==79) selected=count-1;
    else if(scan==72) selected=selected<=0?count-1:selected-1;
    else selected=(selected+1)%count;
    if(page==PAGE_CPU) cfg_select_cpu(selected); else select_memory(selected);
   } else if(scan==72) focus=focus?focus-1:n+2;
   else if(scan==80) focus=(focus+1)%(n+3);
   else if(page==PAGE_MIXER && focus<n && (scan==75 || scan==77)) step_volume(&fields[focus],scan==75?-1:1);
   else if(ch==13 || ch==' ') {
    if(focus==n) action=3; else if(focus==n+1) action=1; else if(focus==n+2) action=2;
    else if(page!=PAGE_CPU && page!=PAGE_MEMORY) edit_field(&fields[focus]);
   }
  }
  if(mouse_present) {
   mouse_call(3,&r); x=r.x.cx/8; y=r.x.dx/8;
   if((r.x.bx&1) && !(previous&1)) {
    redraw=1;
    if(y==2 && x>=4 && x<4+PAGE_COUNT*TAB_STEP && (x-4)%TAB_STEP<TAB_WIDTH) { page=(x-4)/TAB_STEP; focus=0; }
    else if(y==22 && x>=4 && x<32) action=3;
    else if(y==22 && x>=44 && x<58) action=1;
    else if(y==22 && x>=60 && x<75) action=2;
    else if(x>=65 && x<73 && y==4 && field_offset(n,focus)>0) focus=field_offset(n,focus)-1;
    else if(x>=65 && x<73 && y==16 && field_offset(n,focus)+FORM_VISIBLE_FIELDS<n)
     focus=field_offset(n,focus)+FORM_VISIBLE_FIELDS;
    else if(x>=5 && x<75 && y>=5) {
     if(page==PAGE_CPU && y<5+PROFILE_COUNT) { cfg_select_cpu(y-5); focus=0; }
     else if(page==PAGE_MEMORY && y<5+MEMORY_COUNT) { select_memory(y-5); focus=0; }
     else if(page!=PAGE_CPU && page!=PAGE_MEMORY && y<5+FORM_VISIBLE_FIELDS && y-5+field_offset(n,focus)<n) {
      focus=y-5+field_offset(n,focus);
      if(page==PAGE_MIXER && x>=37 && x<42) step_volume(&fields[focus],-1);
      else if(page==PAGE_MIXER && x>=69) step_volume(&fields[focus],1);
      else edit_field(&fields[focus]);
     }
    }
   }
   previous=r.x.bx;
  }
  if(action==3) { cfg_reset_page(page); focus=n; redraw=1; }
  if(action==2) { mouse_visible(0); live_cancel(); screen_end(); return 0; }
  if(action==1) {
   if(cfg_save_changes(path)) {
    message("Configuration saved","Live mixer levels kept. Other changes apply after REBOOT. Backup: .BAK.");
    screen_end(); puts("Saved to PIDOS.CFG. Mixer preview kept; run REBOOT for other changes."); return 0;
   }
   message("Unable to save",cfg_error); redraw=1;
  }
  if(!key) int86(0x28,&r,&r);
 }
}
static void clock_values(char *date,char *clock) {
 struct dosdate_t d; struct dostime_t t;
 _dos_getdate(&d); _dos_gettime(&t);
 sprintf(date,"%04u-%02u-%02u",d.year,d.month,d.day);
 sprintf(clock,"%02u:%02u:%02u",t.hour,t.minute,t.second);
}
static void edit_clock(int date_field) {
 struct dosdate_t d; struct dostime_t t;
 char date[CFG_VALUE_SIZE],clock[CFG_VALUE_SIZE],*value;
 unsigned int a,b,c,i; int valid;
 clock_values(date,clock); value=date_field?date:clock;
 if(!input_dialog(date_field?"Date (YYYY-MM-DD)":"Time (HH:MM:SS)",value)) return;
 valid=strlen(value)==(date_field?10:8);
 for(i=0;valid && value[i];++i) {
  if(i==(date_field?4:2) || i==(date_field?7:5)) {
   if(value[i]!=(date_field?'-':':')) valid=0;
  } else if(!isdigit((unsigned char)value[i])) valid=0;
 }
 if(valid && date_field) {
  valid=sscanf(value,"%u-%u-%u",&a,&b,&c)==3 && a>=1980 && a<=2099 && b>=1 && b<=12 && c>=1 && c<=31;
  if(valid) { _dos_getdate(&d); d.year=a; d.month=(unsigned char)b; d.day=(unsigned char)c; valid=!_dos_setdate(&d); }
 } else if(valid) {
  valid=sscanf(value,"%u:%u:%u",&a,&b,&c)==3 && a<24 && b<60 && c<60;
  if(valid) { t.hour=(unsigned char)a; t.minute=(unsigned char)b; t.second=(unsigned char)c; t.hsecond=0; valid=!_dos_settime(&t); }
 }
 if(!valid) message("Invalid date/time",date_field?"Use a valid date from 1980-01-01 through 2099-12-31.":"Use a time from 00:00:00 through 23:59:59.");
}
/* The post-reboot review uses the selected layout, country and host timezone.
 * DOS date/time edits deliberately retain the standard session-only behavior. */
static int first_boot_review(const char *path) {
 int focus=0,redraw=1,x,y,previous=1,action; unsigned int key,ch,scan;
 char date[CFG_VALUE_SIZE],clock[CFG_VALUE_SIZE],last[CFG_VALUE_SIZE]="",sample[CFG_VALUE_SIZE]="",line[160];
 union REGS r;
 screen_start();
 for(;;) {
  clock_values(date,clock);
  if(strcmp(clock,last)) { strcpy(last,clock); redraw=1; }
  if(redraw) {
   mouse_visible(0); fill(0,0,80,25,NORMAL); fill(0,0,80,1,SELECTED);
   text(3,0,"Welcome to Pi-DOS " PI_DOS_VERSION " - keyboard and clock",SELECTED,73);
   text(5,2,"Your regional and display settings are active. Check them below.",PANEL,70);
   box(3,4,74,13);
   sprintf(line,"Keyboard: %.20s    Country: %.20s",cfg_get(K_KEYBOARD),cfg_get(K_COUNTRY)); text(6,5,line,PANEL,68);
   sprintf(line,"Timezone: %.63s",cfg_get(K_TIMEZONE)); text(6,6,line,PANEL,68);
   fill(5,8,70,1,focus==0?SELECTED:NORMAL); text(7,8,"Test keyboard...",focus==0?SELECTED:NORMAL,65);
   fill(5,10,70,1,focus==1?SELECTED:NORMAL); text(7,10,"Date (YYYY-MM-DD)",focus==1?SELECTED:NORMAL,28); text(38,10,date,focus==1?SELECTED:NORMAL,30);
   fill(5,12,70,1,focus==2?SELECTED:NORMAL); text(7,12,"Time (HH:MM:SS)",focus==2?SELECTED:NORMAL,28); text(38,12,clock,focus==2?SELECTED:NORMAL,30);
   text(6,15,"Enter edits a field. The typing test does not save typed text.",DIM,68);
   text(4,18,"Wired network time may correct the clock after startup.",PANEL,72);
   text(4,19,"Manual date/time changes apply to this DOS session only.",DIM,72);
   text(4,20,"Later: PIDOSCFG changes settings; DATE and TIME adjust the DOS clock.",DIM,72);
   fill(43,22,32,1,focus==3?FOCUSED:BUTTON); text(43,22,"    Finish / keep clock (F2)    ",focus==3?FOCUSED:BUTTON,32);
   fill(0,24,80,1,SELECTED); text(2,24,"Tab/arrows: focus   Enter: select   Esc: finish with current clock",SELECTED,76);
   mouse_visible(1); redraw=0;
  }
  key=key_read(0); ch=key&255; scan=key>>8; action=-1;
  if(key) {
   redraw=1;
   if(ch==27 || scan==60) action=3;
   else if(ch==9 || scan==80) focus=(focus+1)%4;
   else if(scan==72 || (scan==15 && !ch)) focus=(focus+3)%4;
   else if(ch==13 || ch==' ') action=focus;
  }
  if(mouse_present) {
   mouse_call(3,&r); x=r.x.cx/8; y=r.x.dx/8;
   if((r.x.bx&1) && !(previous&1)) {
    if(x>=5 && x<75 && (y==8 || y==10 || y==12)) { focus=(y-8)/2; action=focus; }
    else if(y==22 && x>=43 && x<75) action=3;
   }
   previous=r.x.bx;
  }
  if(action==0) { input_dialog_mode("Keyboard typing test",sample,1); redraw=1; }
  else if(action==1 || action==2) { edit_clock(action==1); redraw=1; }
  else if(action==3) {
   if(cfg_set(K_FIRST_BOOT,"done") && cfg_save_changes(path)) { screen_end(); return 0; }
   message("Unable to finish setup",cfg_error); redraw=1;
  }
  if(!key) int86(0x28,&r,&r);
 }
}
static int first_boot(const char *path) {
 Field fields[5];
 int n,focus=0,redraw=1,x,y,previous=1,action,i; unsigned int key,ch,scan;
 char value[CFG_VALUE_SIZE]; union REGS r;
 if(equal(cfg_get(K_FIRST_BOOT),"clock")) return first_boot_review(path);
 screen_start();
 for(;;) {
  n=form_first_boot_fields(fields);
  if(focus>=n+2) focus=n;
  if(redraw) {
   mouse_visible(0); fill(0,0,80,25,NORMAL); fill(0,0,80,1,SELECTED);
   text(3,0,"Welcome to Pi-DOS " PI_DOS_VERSION,SELECTED,73);
   text(5,2,"Choose your regional settings and monitor, or keep these defaults.",PANEL,70);
   box(3,4,74,13);
   for(i=0;i<n;++i) {
    unsigned char color=focus==i?SELECTED:NORMAL;
    field_value(&fields[i],value); fill(5,6+i*2,70,1,color);
    text(7,6+i*2,fields[i].label,color,28); text(37,6+i*2,value,color,37);
   }
   sprintf(value,"CPU: %s",cfg_profile()<0?"Custom settings":cpu_profiles[cfg_profile()].label);
   text(7,15,value,DIM,65);
   text(4,18,focus<n?fields[focus].help:focus==n?"Save settings and reboot, then test the keyboard and review local time.":
    "Skip discards pending changes and keeps the saved defaults.",PANEL,72);
   text(4,19,"Keyboard, country and timezone can be selected independently.",DIM,72);
   text(4,20,"You can change these and other settings later by running PIDOSCFG.",DIM,72);
   fill(24,22,32,1,focus==n?FOCUSED:BUTTON); text(24,22,"    Save and reboot (F2)        ",focus==n?FOCUSED:BUTTON,32);
   fill(59,22,16,1,focus==n+1?FOCUSED:BUTTON); text(59,22,"  Skip (Esc)    ",focus==n+1?FOCUSED:BUTTON,16);
   fill(0,24,80,1,SELECTED); text(2,24,"Tab/arrows: focus   Enter: select   F2: save and reboot   Esc: skip",SELECTED,76);
   mouse_visible(1); redraw=0;
  }
  key=key_read(0); ch=key&255; scan=key>>8; action=0;
  if(key) {
   redraw=1;
   if(ch==27) action=2;
   else if(scan==60) action=1;
   else if(ch==9 || scan==80) focus=(focus+1)%(n+2);
   else if(scan==72 || (scan==15 && !ch)) focus=(focus+n+1)%(n+2);
   else if(ch==13 || ch==' ') {
    if(focus==n) action=1;
    else if(focus==n+1) action=2;
    else edit_field(&fields[focus]);
   }
  }
  if(mouse_present) {
   mouse_call(3,&r); x=r.x.cx/8; y=r.x.dx/8;
   if((r.x.bx&1) && !(previous&1)) {
    redraw=1;
    if(y==22 && x>=24 && x<56) action=1;
    else if(y==22 && x>=59 && x<75) action=2;
    else if(x>=5 && x<75 && y>=6 && y<6+n*2 && !(y%2)) {
     focus=(y-6)/2; edit_field(&fields[focus]);
    }
   }
   previous=r.x.bx;
  }
  if(action) {
   /* Skip never writes any pending regional/display edits. */
   if((action!=2 || cfg_load(path)) && cfg_set(K_FIRST_BOOT,action==1?"clock":"done") && cfg_save_changes(path)) {
    screen_end();
    if(action==1) puts("Settings saved. Reboot to apply; keyboard and clock review follows.");
    return action==1?10:0;
   }
   message("Unable to save setup",cfg_error); redraw=1;
  }
  if(!key) int86(0x28,&r,&r);
 }
}
static void usage(void)
{
    puts("Pi-DOS " PI_DOS_VERSION " Setup - hardware, network, mixer and system settings\n"
         "PIDOSCFG                         Open keyboard/mouse setup\n"
         "PIDOSCFG /LIST                   List profiles\n"
         "PIDOSCFG /PROFILE id             Save a profile without opening setup\n"
         "PIDOSCFG /FIRSTBOOT              Run the welcome / pending clock review\n"
         "Optional: /CONFIG path           Use another configuration file\n"
         "Mixer can apply live; other changes need reboot. MHz labels are approximate.\n"
         "The previous configuration is backed up with a .BAK extension.\n"
         "Built with Open Watcom; runtime source and license: C:\\DOCS\\WATCOM.TXT");
}

int main(int argc, char **argv)
{
    const char *path = DEFAULT_CONFIG, *profile = NULL;
    int i, chosen = -1, list = 0, welcome = 0;
    for (i = 1; i < argc; ++i) {
        if (equal(argv[i], "/?") || equal(argv[i], "/HELP")) { usage(); return 0; }
        if (equal(argv[i], "/LIST")) list = 1;
        else if (equal(argv[i], "/FIRSTBOOT")) welcome = 1;
        else if (equal(argv[i], "/PROFILE") && i+1 < argc) profile = argv[++i];
        else if (equal(argv[i], "/CONFIG") && i+1 < argc) path = argv[++i];
        else { usage(); return 1; }
    }
    if (welcome && (list || profile)) { usage(); return 1; }
    if (list) {
        for (i = 0; i < PROFILE_COUNT; ++i)
            printf("%-12s %-29s %s\n", cpu_profiles[i].id,
                   cpu_profiles[i].label, cpu_profiles[i].cycles);
        return 0;
    }
    if (profile) {
        for (i = 0; i < PROFILE_COUNT; ++i)
            if (equal(profile, cpu_profiles[i].id)) chosen = i;
        if (chosen < 0) { puts("Unknown profile. Run PIDOSCFG /LIST."); return 1; }
    }
    if (!cfg_load(path)) { puts(cfg_error); return 1; }
    if (profile) {
        if (!cfg_save(path, chosen)) { puts(cfg_error); return 1; }
        printf("Saved %s. Restart Pi-DOS to apply.\n", cpu_profiles[chosen].label);
        return 0;
    }
    {
        const char *last=strrchr(argv[0],'\\');
        if(last && (unsigned int)(last-argv[0]+1)<sizeof(catalog_dir)) {
            memcpy(catalog_dir,argv[0],last-argv[0]+1); catalog_dir[last-argv[0]+1]=0;
        }
    }
    return welcome ? first_boot(path) : setup(path);
}
