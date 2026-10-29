#ifndef PI_DOS_NETWORK_OPTIONS_H
#define PI_DOS_NETWORK_OPTIONS_H

/* Shared C89 validation for DOS setup and generated startup commands. */
#include <ctype.h>
#include <stdlib.h>
#include <string.h>

static int pidos_net_port(const char *value)
{
 unsigned long port=0;
 if(!*value) return 0;
 while(*value) {
  if(*value<'0' || *value>'9') return 0;
  port=port*10+(*value++-'0'); if(port>65535UL) return 0;
 }
 return port>0;
}
static int pidos_net_host(const char *value)
{
 unsigned int i;
 if(!*value || strlen(value)>63 || !isalnum((unsigned char)*value)) return 0;
 for(i=0;value[i];++i)
  if(!isalnum((unsigned char)value[i]) && value[i]!='.' && value[i]!='-') return 0;
 return 1;
}
static int pidos_net_ipv4(const char *value)
{
 unsigned int part=0,digits=0,count=0;
 for(;;++value) {
  if(*value>='0' && *value<='9') {
   part=part*10+(*value-'0'); if(++digits>3 || part>255) return 0;
  } else {
   if(!digits) return 0;
   ++count;
   if(!*value) return count==4;
   if(*value!='.' || count>=4) return 0;
   part=digits=0;
  }
 }
}
static int pidos_net_mac(const char *value)
{
 unsigned int i; unsigned long first;
 if(!strcmp(value,"random")) return 1;
 if(strlen(value)!=17) return 0;
 for(i=0;i<17;++i)
  if(i%3==2 ? value[i]!=':' : !isxdigit((unsigned char)value[i])) return 0;
 first=strtoul(value,NULL,16);
 return !(first&1) && strcmp(value,"00:00:00:00:00:00");
}
static int pidos_net_interface(const char *value)
{
 unsigned int i;
 if(!*value || strlen(value)>15) return 0;
 for(i=0;value[i];++i)
  if(!isalnum((unsigned char)value[i]) && !strchr("_.:-",value[i])) return 0;
 return 1;
}
static int pidos_net_dos_path(const char *value)
{
 unsigned int i;
 if(strlen(value)<3 || strlen(value)>60 || !isalpha((unsigned char)value[0]) ||
    value[1]!=':' || value[2]!='\\' || strstr(value,"..")) return 0;
 for(i=3;value[i];++i)
  if(!isalnum((unsigned char)value[i]) && !strchr("_\\-",value[i])) return 0;
 return value[strlen(value)-1]!='\\';
}
/* Space-separated host[:guest] ports; ranges can be entered in EDIT. */
static int pidos_net_forwards(const char *value)
{
 char port[6]; unsigned int n; int side;
 while(*value) {
  while(*value==' ') ++value;
  if(!*value) break;
  for(side=0;side<2;++side) {
   n=0;
   while(*value>='0' && *value<='9') {
    if(n>=5) return 0;
    port[n++]=*value++;
   }
   port[n]=0; if(!pidos_net_port(port)) return 0;
   if(side==0 && *value==':') { ++value; continue; }
   break;
  }
  if(*value && *value!=' ') return 0;
 }
 return 1;
}
#endif
