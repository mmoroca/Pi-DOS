/* Real DOS API methods; PSP memory, devices and disk transfers are stand-ins. */
#include <algorithm>
#include <cassert>
#include <cctype>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <memory>
#include <string>
#include <vector>
#include "dos_sharing.h"
#include "dos_attributes_linux.h"
using Bits=long;using RealPt=uint32_t;
#define DOS_FILES 64
#define DOS_DRIVES 26
#define DOS_DEVICES 1
#define DOS_PATHLENGTH 255
#define CROSS_LEN 4096
#define DOS_NAMELENGTH_ASCII 13
#define LFN_NAMELENGTH 255
#define LFN_FILEFIND_MAX 255
#define LFN_FILEFIND_INTERNAL 256
#define DOS_SEEK_SET 0
#define DOS_SEEK_CUR 1
#define DOS_SEEK_END 2
#define DOS_NOT_INHERIT 128
#define LOG_MSG(...) ((void)0)
static void fake_log(const char*,...){}
#define LOG(...) fake_log
static void E_Exit(const char*){assert(false);}
enum {OPEN_READ=0,OPEN_WRITE=1,OPEN_READWRITE=2,OPEN_READ_NO_MOD=4};
enum {DOS_ATTR_READ_ONLY=1,DOS_ATTR_HIDDEN=2,DOS_ATTR_SYSTEM=4,DOS_ATTR_VOLUME=8,DOS_ATTR_DIRECTORY=16,DOS_ATTR_ARCHIVE=32};
enum {DOSERR_FUNCTION_NUMBER_INVALID=1,DOSERR_FILE_NOT_FOUND=2,DOSERR_PATH_NOT_FOUND=3,DOSERR_TOO_MANY_OPEN_FILES=4,DOSERR_ACCESS_DENIED=5,DOSERR_INVALID_HANDLE=6,DOSERR_NOT_SAME_DEVICE=17,DOSERR_ACCESS_CODE_INVALID=12,DOSERR_WRITE_PROTECTED=19,DOSERR_DRIVE_NOT_READY=21,DOSERR_FILE_ALREADY_EXISTS=80};
namespace DeviceInfoFlags {constexpr unsigned ExternalDevice=0x8000;}
static bool enable_share_exe=true,log_fileio=false,dos_kernel_disabled=false,uselfn=false,force_sfn=false;
static int dos_clipboard_device_access=0,lfn_filefind_handle=258;
static const char *dos_clipboard_device_name="CLIP$";
struct Control{bool SecureMode(){return true;}}control_object;static Control *control=&control_object;
static struct {uint16_t current=0x1000,errorcode=0;struct{unsigned tempdta=1;}tables;struct{unsigned major=5;}version;unsigned address=0;uint16_t psp(){return current;}unsigned dta(){return address;}void dta(unsigned p){address=p;}}dos;
static void DOS_SetError(uint16_t e){dos.errorcode=e;}
static unsigned Real2Phys(unsigned p){return p;}
static uint8_t memory[128]={};
static void MEM_BlockRead(unsigned p,void *out,size_t n){memcpy(out,memory+p,n);}
static void MEM_BlockWrite(unsigned p,const void *in,size_t n){memcpy(memory+p,in,n);}
static struct {char name[13]="SAVED";unsigned size=123;}fd;
static char *strrchr_dbcs(char *p,char c){return strrchr(p,c);}
static char *upcase(char *p){for(char *s=p;*s;++s)*s=char(toupper((unsigned char)*s));return p;}
static char *DBCS_upcase(char *p){return upcase(p);}
static std::string canonical(const char *p){std::string s(p);s.erase(std::remove(s.begin(),s.end(),'"'),s.end());if(s.size()<2||s[1]!=':')s="C:\\"+s;return s;}
static std::string alias(std::string s){auto a=s.find("LONG FILE.TXT");if(a!=std::string::npos)s.replace(a,13,"LONGFI~1.TXT");return s;}
static bool DOS_GetSFNPath(const char *p,char *out,bool){std::string s=canonical(p);std::transform(s.begin(),s.end(),s.begin(),[](unsigned char c){return char(toupper(c));});strcpy(out,alias(s).c_str());memset(memory+1,0xee,43);strcpy(fd.name,"CHANGED");return true;}
static bool DOS_Canonicalize(const char *p,char *out){strcpy(out,canonical(p).c_str());return true;}
struct DiskFile{uint32_t length=256;uint16_t attr=32;std::map<uint32_t,uint8_t> data;};
class DOS_File {
public:
    std::shared_ptr<pidos_sharing::Open> sharing;
    std::shared_ptr<DiskFile> disk;
    std::string filename;const char *name=nullptr;
    uint32_t flags=0,position=0;uint8_t drive=2;Bits refCtr=0;
    bool open=true,neverclose=false,ready=true,reports_position=true;
    explicit DOS_File(std::string n="DEVICE",std::shared_ptr<DiskFile> d=std::make_shared<DiskFile>()):disk(d),filename(n),name(filename.c_str()){}
    virtual ~DOS_File()=default;
    bool IsOpen(){return open;}bool SharingReady(){if(!ready)DOS_SetError(21);return ready;}bool SharingRefresh(){return true;}
    void SharingAbandon(){}
    void SetDrive(uint8_t n){drive=n;}uint8_t GetDrive(){return drive;}
    void AddRef(){++refCtr;}Bits RemoveRef(){return --refCtr;}
    uint32_t GetSeekPos(){return reports_position?position:0xffffffffu;}
    virtual uint16_t GetInformation(){return 0;}
    virtual bool LockFile(uint8_t,uint32_t,uint16_t){DOS_SetError(1);return false;}
    virtual bool Close(){if(refCtr<=1)open=false;return ready;}
    virtual bool Seek(uint32_t *p,uint32_t type){uint64_t next=type==2?uint64_t(disk->length)+*p:type==1?uint64_t(position)+*p:*p;position=uint32_t(next);*p=position;return true;}
    virtual bool Read(uint8_t *data,uint16_t *size){if(!SharingReady())return false;if((flags&15)==1){DOS_SetError(5);return false;}*size=uint16_t(std::min(uint32_t(*size),position>=disk->length?0:disk->length-position));for(unsigned i=0;i<*size;++i)data[i]=disk->data[position++];return true;}
    virtual bool Write(const uint8_t *data,uint16_t *size){if(!SharingReady())return false;if((flags&15)==0||(flags&15)==4){DOS_SetError(5);return false;}if(!*size){disk->length=position;return true;}for(unsigned i=0;i<*size;++i)disk->data[position++]=data[i];disk->length=std::max(disk->length,position);return true;}
};
struct DOS_Device:DOS_File {DOS_Device():DOS_File(){}explicit DOS_Device(const DOS_Device &):DOS_File(){}};
struct DOS_ExtDevice:DOS_Device{explicit DOS_ExtDevice(const DOS_ExtDevice &d):DOS_Device(d){}};
static DOS_File *file_slots[DOS_FILES]={};static DOS_File **Files=file_slots;
static DOS_Device base_device;static DOS_Device *Devices[DOS_DEVICES]={&base_device};
static std::map<uint16_t,std::vector<uint8_t>> jfts;
struct DOS_PSP {
    uint16_t owner;explicit DOS_PSP(uint16_t n):owner(n){if(!jfts.count(n))jfts[n]=std::vector<uint8_t>(64,255);}
    uint8_t FindFreeFileEntry(){auto &v=jfts[owner];for(unsigned i=0;i<v.size();++i)if(v[i]==255)return uint8_t(i);return 255;}
    void SetFileHandle(uint16_t h,uint8_t s){jfts[owner].at(h)=s;}
};
static uint8_t RealHandle(uint16_t n){auto &v=jfts[dos.current];return n<v.size()?v[n]:255;}
struct Drive {
    uint64_t media=pidos_sharing::next_media_id();bool available=true;
    std::map<std::string,std::shared_ptr<DiskFile>> files;
    void EmptyCache(){}
    uint64_t SharingMediaId(){if(!available){DOS_SetError(21);return 0;}return media;}
    std::string key(const char *p){std::string s(p);std::transform(s.begin(),s.end(),s.begin(),[](unsigned char c){return char(toupper(c));});return alias(s);}
    const char *GetInfo(){return "local directory fixture";}
    bool GetFileAttr(const char *p,uint16_t *a){auto it=files.find(key(p));if(it==files.end()){DOS_SetError(2);return false;}*a=it->second->attr;return true;}
    bool FileExists(const char *p){return files.count(key(p));}
    bool FileOpen(DOS_File **f,const char *p,uint32_t flags){auto it=files.find(key(p));if(it==files.end()){DOS_SetError(2);return false;}*f=new DOS_File(key(p),it->second);(*f)->flags=flags;return true;}
    bool FileCreate(DOS_File **f,const char *p,uint16_t){auto &d=files[key(p)];if(!d)d=std::make_shared<DiskFile>();d->length=0;*f=new DOS_File(key(p),d);(*f)->flags=2;return true;}
    bool FileUnlink(const char *p){return files.erase(key(p))!=0;}
    bool Rename(const char *old,const char *next){auto it=files.find(key(old));if(it==files.end())return false;files[key(next)]=it->second;files.erase(it);return true;}
};
static Drive *Drives[DOS_DRIVES]={};
static uint8_t DOS_FindDevice(const char *p){return !strcasecmp(p,"NUL")?0:DOS_DEVICES;}
static bool DOS_MakeName(const char *p,char *out,uint8_t *drive){std::string s=canonical(p);*drive=uint8_t(toupper(s[0])-'A');if(*drive>=26||!Drives[*drive]){DOS_SetError(15);return false;}strcpy(out,s.c_str()+3);return true;}
static bool DOS_GetFileAttr(const char *p,uint16_t *a){uint8_t d;char n[256];return DOS_MakeName(p,n,&d)&&Drives[d]->GetFileAttr(n,a);}
static bool PathExists(const char*){return true;}
struct DOS_DTA{explicit DOS_DTA(unsigned){}void GetResult(char*,char*,uint32_t&,uint32_t&,uint16_t&,uint16_t&,uint8_t&){assert(false);}};
static bool DOS_FindFirst(const char*,uint16_t){return false;}static bool DOS_FindNext(){return false;}
static pidos_sharing::Table dos_sharing;
bool DOS_CloseFile(uint16_t,bool=false,uint8_t * = nullptr);
bool DOS_ReadFile(uint16_t,uint8_t*,uint16_t*,bool=false);
bool DOS_WriteFile(uint16_t,const uint8_t*,uint16_t*,bool=false);
bool DOS_SeekFile(uint16_t,uint32_t*,uint32_t,bool=false);
bool DOS_OpenFile(const char*,uint8_t,uint16_t*,bool=false);
bool DOS_CreateFile(const char*,uint16_t,uint16_t*,bool=false,uint8_t=2);
#include "methods.h"
static void select_psp(uint16_t p){dos.current=p;DOS_PSP init(p);}
static uint16_t open_file(const char *path,uint8_t flags){uint16_t h=255;assert(DOS_OpenFile(path,flags,&h));return h;}
static bool write_byte(uint16_t h,uint32_t at){uint32_t p=at;assert(DOS_SeekFile(h,&p,0));uint8_t b=7;uint16_t n=1;return DOS_WriteFile(h,&b,&n);}
static void terminate(uint16_t owner){select_psp(owner);for(unsigned h=0;h<64;++h)if(RealHandle(h)!=255)DOS_CloseFile(uint16_t(h));DOS_ReleaseSharingLocks(owner);}
static void inherit(uint16_t parent,uint16_t child){select_psp(child);for(unsigned h=0;h<64;++h){auto s=jfts[parent][h];if(s<64&&Files[s]&&!(Files[s]->flags&DOS_NOT_INHERIT)){jfts[child][h]=s;Files[s]->AddRef();}}}
int main(int argc,char **argv){
    assert(argc==2);Drive c,a;c.files["FILE.DAT"]=std::make_shared<DiskFile>();c.files["LONGFI~1.TXT"]=std::make_shared<DiskFile>();c.files["SUB"]=std::make_shared<DiskFile>();c.files["SUB"]->attr=16;c.files["SUB\\DATA"]=std::make_shared<DiskFile>();a.files=c.files;Drives[0]=&a;Drives[2]=Drives[4]=&c;select_psp(0x1000);
    // Every pair of explicit share modes/accesses, in both PSPs.
    for(unsigned old_mode=1;old_mode<=4;++old_mode)for(unsigned old_access=0;old_access<3;++old_access)
    for(unsigned mode=1;mode<=4;++mode)for(unsigned access=0;access<3;++access){
        select_psp(0x1000);uint16_t first=open_file("C:\\FILE.DAT",uint8_t(old_mode*16+old_access));select_psp(0x2000);
        uint16_t next=0;bool ok=DOS_OpenFile("C:\\FILE.DAT",uint8_t(mode*16+access),&next);
        const unsigned deny[]={0,3,2,1,0};const unsigned readwrite[]={1,2,3};
        bool expected=!(deny[old_mode]&readwrite[access])&&!(deny[mode]&readwrite[old_access]);assert(ok==expected);if(ok)DOS_CloseFile(next);else assert(dos.errorcode==32);
        select_psp(0x1000);DOS_CloseFile(first);
    }
    uint16_t first=open_file("C:\\FILE.DAT",0x12),other=0;
    assert(!DOS_OpenFile("C:\\FILE.DAT",0x40,&other)&&dos.errorcode==32);
    uint16_t status=0;const auto old_length=c.files["FILE.DAT"]->length;
    assert(!DOS_OpenFileExtended("C:\\FILE.DAT",0x40,0,0x12,&other,&status)&&dos.errorcode==32&&c.files["FILE.DAT"]->length==old_length);
    assert(!DOS_UnlinkFile("C:\\FILE.DAT")&&dos.errorcode==32);assert(!DOS_Rename("C:\\FILE.DAT","C:\\RENAMED")&&dos.errorcode==32);
    uint16_t duplicate;assert(DOS_DuplicateEntry(first,&duplicate));DOS_CloseFile(first);assert(!DOS_OpenFile("C:\\FILE.DAT",0x40,&other));DOS_CloseFile(duplicate);
    first=open_file("C:\\Long File.txt",0x12);assert(!DOS_OpenFile("C:\\LONGFI~1.TXT",0x40,&other));DOS_CloseFile(first);
    // Compatibility opens retain DOS's legacy behavior when mixed with sharing.
    first=open_file("C:\\FILE.DAT",2);other=open_file("C:\\FILE.DAT",0);DOS_CloseFile(other);
    assert(!DOS_OpenFile("C:\\FILE.DAT",0x40,&other)&&dos.errorcode==32);DOS_CloseFile(first);
    first=open_file("C:\\FILE.DAT",0);assert(!DOS_OpenFile("C:\\FILE.DAT",0x40,&other)&&dos.errorcode==32);
    c.files["FILE.DAT"]->attr|=DOS_ATTR_READ_ONLY;other=open_file("C:\\FILE.DAT",0x40);DOS_CloseFile(other);c.files["FILE.DAT"]->attr&=~DOS_ATTR_READ_ONLY;
    assert(!DOS_OpenFileExtended("C:\\FILE.DAT",0x22,0,0x12,&other,&status)&&dos.errorcode==32&&c.files["FILE.DAT"]->length==old_length);DOS_CloseFile(first);
    dos.version.major=7;first=open_file("C:\\FILE.DAT",0);other=open_file("C:\\FILE.DAT",0x40);DOS_CloseFile(other);DOS_CloseFile(first);dos.version.major=5;
    assert(!DOS_OpenFile("C:\\FILE.DAT",0x50,&first)&&dos.errorcode==12);
    first=open_file("C:\\SUB\\DATA",0x40);assert(!DOS_Rename("C:\\SUB","C:\\NEXT")&&dos.errorcode==32);DOS_CloseFile(first);
    // Independent opens can perform I/O, but other DOS owners cannot touch locks.
    first=open_file("C:\\FILE.DAT",0x42);assert(DOS_LockFile(first,0,16,32));assert(write_byte(first,16));
    assert(DOS_LockFile(first,0,96,8));assert(DOS_LockFile(first,1,96,8));
    select_psp(0x2000);other=open_file("C:\\FILE.DAT",0x42);uint8_t buffer[64];uint16_t n=64;
    assert(!DOS_ReadFile(other,buffer,&n)&&dos.errorcode==33&&n==0);assert(!write_byte(other,20)&&dos.errorcode==33);assert(write_byte(other,64));
    uint32_t p=8;assert(DOS_SeekFile(other,&p,0));n=0;assert(!DOS_WriteFile(other,buffer,&n)&&dos.errorcode==33&&c.files["FILE.DAT"]->length==old_length);
    assert(!DOS_LockFile(other,0,24,1)&&dos.errorcode==33);assert(!DOS_LockFile(other,1,16,32)&&dos.errorcode==33);
    select_psp(0x1000);assert(!DOS_LockFile(first,0,16,32)&&dos.errorcode==33);assert(!DOS_LockFile(first,1,16,16));
    // An attribute query opening/closing the host file cannot drop DOS locks.
    FILE *metadata=fopen(argv[1],"wb");assert(metadata);fclose(metadata);assert(pidos_attributes::with_file(argv[1],[](int){return true;}));
    select_psp(0x2000);assert(!write_byte(other,16)&&dos.errorcode==33);select_psp(0x1000);assert(DOS_LockFile(first,1,16,32));
    assert(DOS_LockFile(first,0,0x80000000u,65536));select_psp(0x2000);assert(!write_byte(other,0x80000000u));p=0x80010000u;assert(DOS_SeekFile(other,&p,0));n=1;assert(DOS_ReadFile(other,buffer,&n));
    select_psp(0x1000);assert(DOS_LockFile(first,1,0x80000000u,65536));assert(DOS_LockFile(first,0,0xffffffffu,1));assert(!DOS_LockFile(first,0,0xffffffffu,2));assert(DOS_LockFile(first,1,0xffffffffu,1));
    assert(!DOS_LockFile(first,2,0,1)&&dos.errorcode==1);
    assert(DOS_LockFile(first,0,0,0));select_psp(0x2000);assert(write_byte(other,0));select_psp(0x1000);
    // Inheritance preserves the grant, but record-lock ownership remains the PSP.
    assert(DOS_LockFile(first,0,0,8));inherit(0x1000,0x3000);assert(!write_byte(first,0));assert(DOS_LockFile(first,0,64,8));terminate(0x3000);
    select_psp(0x2000);assert(!write_byte(other,0));assert(write_byte(other,64));
    select_psp(0x1000);assert(DOS_DuplicateEntry(first,&duplicate));DOS_CloseFile(first);assert(DOS_LockFile(duplicate,1,0,8));DOS_CloseFile(duplicate);select_psp(0x2000);DOS_CloseFile(other);
    // New creates honour extended-open access/sharing flags, including inheritance.
    select_psp(0x1000);assert(DOS_OpenFileExtended("C:\\NEW.DAT",0x90,0,0x10,&first,&status)&&status==2);
    assert((Files[RealHandle(first)]->flags&0xff)==0x90);assert(!write_byte(first,0));inherit(0x1000,0x3000);assert(RealHandle(first)==255);select_psp(0x1000);DOS_CloseFile(first);
    // Open-existing/fail-if-exists must not leak the temporary grant or handle.
    assert(!DOS_OpenFileExtended("C:\\NEW.DAT",0x12,0,0x10,&first,&status)&&dos.errorcode==80);first=open_file("C:\\NEW.DAT",0x12);DOS_CloseFile(first);
    first=open_file("C:\\FILE.DAT",0x12);uint16_t replacement=open_file("C:\\NEW.DAT",0x12);assert(DOS_ForceDuplicateEntry(first,replacement));other=open_file("C:\\NEW.DAT",0x12);DOS_CloseFile(other);DOS_CloseFile(first);DOS_CloseFile(replacement);
    // Removed media cannot retain a grant over a replacement volume/drive letter.
    first=open_file("A:\\FILE.DAT",0x12);a.media=pidos_sharing::next_media_id();other=open_file("A:\\FILE.DAT",0x12);DOS_CloseFile(other);Files[RealHandle(first)]->ready=false;assert(!DOS_LockFile(first,0,0,1)&&dos.errorcode==21);DOS_CloseFile(first);
    // Disabling SHARE bypasses conflicts; ordinary close still releases grants.
    first=open_file("C:\\FILE.DAT",0x12);enable_share_exe=false;other=open_file("C:\\FILE.DAT",0x42);assert(!DOS_LockFile(other,0,0,1)&&dos.errorcode==1);DOS_CloseFile(other);DOS_CloseFile(first);enable_share_exe=true;
    first=open_file("C:\\FILE.DAT",0x12);terminate(0x1000);select_psp(0x2000);first=open_file("C:\\FILE.DAT",0x42);DOS_CloseFile(first);
    // FCB opens are not in the JFT; exit must close them explicitly.
    select_psp(0x1000);assert(DOS_OpenFile("C:\\FILE.DAT",2,&first,true));select_psp(0x2000);
    assert(!DOS_OpenFile("C:\\FILE.DAT",0x12,&other)&&dos.errorcode==32);terminate(0x1000);
    select_psp(0x2000);other=open_file("C:\\FILE.DAT",0x12);DOS_CloseFile(other);
    select_psp(0x1000);first=open_file("C:\\FILE.DAT",2);assert(DOS_LockFile(first,0,0,8));select_psp(0x2000);
    assert(!DOS_CreateFile("C:\\FILE.DAT",0,&other)&&dos.errorcode==33&&c.files["FILE.DAT"]->length==old_length);
    terminate(0x1000);select_psp(0x2000);
    pidos_sharing::Table bounded;pidos_sharing::Key key;key.media=123;key.path="BOUNDED";
    auto grant=bounded.opened(key,0x42);
    for(unsigned i=0;i<4096;++i)assert(!bounded.lock(grant,1,0,i,1));
    assert(bounded.lock(grant,1,0,4096,1)==36);bounded.release_owner(1);assert(!bounded.lock(grant,2,0,0,1));
    assert(!strcmp(fd.name,"SAVED")&&fd.size==123);for(unsigned i=1;i<44;++i)assert(memory[i]==0);
    // Z: Virtual_File supports Seek but inherits the unknown GetSeekPos sentinel.
    // Boot invokes MIXER.COM eleven times after the welcome message.
    Drive z; z.files["SYSTEM\\MIXER.COM"]=std::make_shared<DiskFile>();Drives[25]=&z;
    select_psp(0x3000);
    for(unsigned i=0;i<11;++i) {
        first=open_file("Z:\\SYSTEM\\MIXER.COM",0);
        Files[RealHandle(first)]->reports_position=false;n=32;
        assert(DOS_ReadFile(first,buffer,&n)&&n==32);
        assert(Files[RealHandle(first)]->position==32);
        n=16;assert(DOS_ReadFile(first,buffer,&n)&&n==16);
        assert(Files[RealHandle(first)]->position==48);
        DOS_CloseFile(first);
    }
    // Unknown cached positions must still honor record locks at the real offset.
    first=open_file("C:\\FILE.DAT",0x42);assert(DOS_LockFile(first,0,16,16));
    select_psp(0x4000);other=open_file("C:\\FILE.DAT",0x42);
    Files[RealHandle(other)]->reports_position=false;
    p=16;assert(DOS_SeekFile(other,&p,0));n=1;
    assert(!DOS_ReadFile(other,buffer,&n)&&dos.errorcode==33&&n==0);
    p=32;assert(DOS_SeekFile(other,&p,0));n=1;assert(DOS_ReadFile(other,buffer,&n));
    DOS_CloseFile(other);terminate(0x3000);
    puts("PASS: real DOS API: sharing/locks, unknown cached positions, eleven boot-program loads, aliases, DTA preservation, dup/inherit/exit and media replacement");
}
