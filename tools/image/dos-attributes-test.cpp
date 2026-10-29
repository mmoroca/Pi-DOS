/* Real local-drive/ATTRIB methods; only DOS plumbing and filesystem ioctls are simulated. */
#include <algorithm>
#include <cassert>
#include <cerrno>
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <filesystem>
#include <fnmatch.h>
#include <map>
#include <string>
#include <vector>
#include <fcntl.h>
#include <linux/msdos_fs.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <unistd.h>
namespace fs = std::filesystem;
static std::map<ino_t,uint32_t> attributes;
static bool fail_get=false,fail_set=false,fail_sync=false;
static unsigned sync_count=0;
static ino_t non_fat_parent=0;
static unsigned parent_attribute_requests=0;
static int mock_ioctl(int fd,unsigned long request,uint32_t *value) {
    struct stat st={};assert(!fstat(fd,&st));
    if(st.st_ino==non_fat_parent){++parent_attribute_requests;errno=ENOTTY;return -1;}
    auto inserted=attributes.emplace(st.st_ino,S_ISDIR(st.st_mode)?0x10:0x20);
    uint32_t &stored=inserted.first->second;
    if(request==FAT_IOCTL_GET_ATTRIBUTES) {
        if(fail_get){errno=EIO;return -1;}
        *value=stored;return 0;
    }
    assert(request==FAT_IOCTL_SET_ATTRIBUTES);
    if(fail_set){errno=EROFS;return -1;}
    stored=*value;return 0;
}
static int mock_fsync(int fd) {++sync_count;if(fail_sync){errno=EIO;return -1;}return ::fsync(fd);}
#define ioctl mock_ioctl
#define fsync mock_fsync
#include "dos_attributes_linux.h"
#undef ioctl
#undef fsync
#define CROSS_LEN 4096
#define DOS_PATHLENGTH 260
#define DOS_NAMELENGTH_ASCII 13
#define LFN_NAMELENGTH 255
#define LFN_FILEFIND_MAX 255
#define DOS_DRIVES 26
#define DOS_FILES 64
#define CROSS_FILESPLIT '/'
#define CROSS_FILENAME(path) do{for(char *p=path;*p;++p)if(*p=='\\')*p='/';}while(false)
#define HELP(name) ((void)0)
#define LOG_MSG(...) ((void)0)
static void fake_log(const char*,...) {}
#define LOG(...) fake_log
using host_cnv_char_t=char;
using ht_stat_t=struct stat;
using Bitu=unsigned;
using RealPt=unsigned;
#define _HT(value) value
#define ht_stat stat
#define ht_unlink unlink
static const char *CodePageGuestToHost(const char *s){return s;}
enum {DOS_ATTR_READ_ONLY=1,DOS_ATTR_HIDDEN=2,DOS_ATTR_SYSTEM=4,DOS_ATTR_VOLUME=8,DOS_ATTR_DIRECTORY=16,DOS_ATTR_ARCHIVE=32};
enum {DOSERR_FILE_NOT_FOUND=2,DOSERR_PATH_NOT_FOUND=3,DOSERR_ACCESS_DENIED=5,DOSERR_ACCESS_CODE_INVALID=12,DOSERR_NO_MORE_FILES=18,DOSERR_WRITE_PROTECTED=19};
enum {OPEN_READ=0,OPEN_WRITE=1,OPEN_READWRITE=2,OPEN_READ_NO_MOD=4};
static int error=0;
static void DOS_SetError(int n){error=n;}
static bool nocachedir=false,enable_share_exe=false,uselfn=false,inshell=false,ctrlbrk=false;
static bool DOS_UsesInternalSharing(){return true;}
static const bool IS_PC98_ARCH=false;
static bool dos_kernel_disabled=false;
static int file_access_tries=0,lfn_filefind_handle=258;
static uint16_t ldid[256]={};static std::string ldir[256];
static bool isDBCSCP(){return false;}static bool isKanji1(char){return false;}
static char *upcase(char *s){for(char *p=s;*p;++p)*p=char(toupper((unsigned char)*p));return s;}
static char *DBCS_upcase(char *s){return upcase(s);}
static bool share(int,int,uint32_t){return true;}
static bool check_last_split_char(const char *s,size_t n,char c){return n&&s[n-1]==c;}
static uint16_t DOS_PackDate(uint16_t,uint16_t,uint16_t){return 1;}
static uint16_t DOS_PackTime(uint16_t,uint16_t,uint16_t){return 2;}
static bool WildFileCmp(const char *name,const char *pattern){return !strcmp(pattern,"*.*")||!fnmatch(pattern,name,FNM_CASEFOLD);}
static bool LWildFileCmp(const char *name,const char *pattern){return WildFileCmp(name,pattern);}
static bool PiDosApplianceDrives(){return true;}
struct DtaState {std::string pattern="*.*",name,lname;uint8_t search=0,attr=0;uint16_t id=0;};
static std::map<unsigned,DtaState> dtas;
struct DOS_DTA {
    unsigned address;explicit DOS_DTA(unsigned p):address(p){}
    DtaState &state(){return dtas[address];}
    uint16_t GetDirID(){return state().id;}void SetDirID(uint16_t n){state().id=n;}
    void GetSearchParams(uint8_t &a,char *p,bool){a=state().search;strcpy(p,state().pattern.c_str());}
    void SetResult(const char *n,const char *l,uint32_t,uint32_t,uint16_t,uint16_t,uint8_t a){state().name=n;state().lname=l;state().attr=a;}
    void GetResult(char *n,char *l,uint32_t &s,uint32_t &h,uint16_t &d,uint16_t &t,uint8_t &a){strcpy(n,state().name.c_str());strcpy(l,state().lname.c_str());s=h=0;d=t=0;a=state().attr;}
};
static struct {uint8_t return_code=0;struct {unsigned tempdta=1;}tables;unsigned current=0;unsigned dta(){return current;}void dta(unsigned p){current=p;}} dos;
struct DOS_File {
    virtual ~DOS_File()=default;uint32_t flags=0;uint16_t attr=0;unsigned drive=0;
    virtual void Flush(){}virtual bool Close(){return true;}bool IsOpen(){return false;}
    bool IsName(const char*){return false;}unsigned GetDrive(){return drive;}int RemoveRef(){return 0;}
};
struct LocalFile: DOS_File {
    FILE *fhandle;
    LocalFile(const char*,FILE *p):fhandle(p){}
    ~LocalFile()override{if(fhandle)fclose(fhandle);}
    bool Close()override{if(!fhandle)return true;FILE *p=fhandle;fhandle=nullptr;return !fclose(p);}
};
static DOS_File *Files[DOS_FILES]={};
class localDrive;static localDrive *Drives[DOS_DRIVES]={};
struct Cache {
    struct Search{std::vector<std::string> names;size_t next=0;};
    std::vector<Search> searches;std::string expanded;
    void ExpandName(char *p){strcpy(p,GetExpandName(p));}
    char *GetExpandName(const char *p){
        fs::path found="/";
        for(const auto &part:fs::path(p).relative_path()){
            bool matched=false;
            if(fs::is_directory(found))for(const auto &item:fs::directory_iterator(found))
                if(!strcasecmp(item.path().filename().c_str(),part.c_str())){found/=item.path().filename();matched=true;break;}
            if(!matched)found/=part;
        }
        expanded=found.string();return const_cast<char*>(expanded.c_str());
    }
    bool FindFirst(const char *p,uint16_t &id){
        std::string dir=GetExpandName(p);if(!fs::is_directory(dir))return false;
        id=uint16_t(searches.size());searches.emplace_back();
        // Linux readdir includes these even at a mounted FAT volume root.
        searches.back().names={".",".."};
        for(const auto &entry:fs::directory_iterator(dir))searches.back().names.push_back(entry.path().filename().string());
        std::sort(searches.back().names.begin(),searches.back().names.end());return true;
    }
    bool FindNext(uint16_t id,char *&n,char *&l){auto &s=searches.at(id);if(s.next==s.names.size())return false;n=l=const_cast<char*>(s.names[s.next++].c_str());return true;}
    void AddEntry(const char*,bool){}void DeleteEntry(const char*,bool=false){}void EmptyCache(){}
    const char *GetLabel(){return "";}
};
class localDrive {
public:
    bool native_dos_attributes=true,readonly=false;char basedir[CROSS_LEN];Cache dirCache;
    struct {unsigned mediaid=0xf8;}allocation;
    struct {char srch_dir[CROSS_LEN];}srchInfo[128]={};
    explicit localDrive(const std::string &p){strcpy(basedir,(p+"/").c_str());}
    bool FileCreate(DOS_File**,const char*,uint16_t);bool FileOpen(DOS_File**,const char*,uint32_t);bool FileUnlink(const char*);
    bool SetFileAttr(const char*,uint16_t);bool GetFileAttr(const char*,uint16_t*);
    bool FindFirst(const char*,DOS_DTA&,bool=false);bool FindNext(DOS_DTA&);
    void EmptyCache(){}bool isRemote(){return false;}bool isRemovable(){return false;}void SetLabel(const char*,bool,bool){}
    const char *GetLabel(){return "";}
    void remove_special_file_from_disk(const char*,const char*){}
    bool add_special_file_to_disk(const char*,const char*,uint16_t,bool){assert(false);return false;}
    std::string create_filename_of_special_operation(const char*,const char*,bool){return "/nonexistent-attribute-companion";}
};
static std::string strip_quotes(std::string s){s.erase(std::remove(s.begin(),s.end(),'"'),s.end());return s;}
static std::string canonical(const char *p){std::string s=strip_quotes(p);if(s.size()<2||s[1]!=':')s="E:\\"+s;else if(s.size()<3||s[2]!='\\')s.insert(2,"\\");return s;}
static bool DOS_Canonicalize(const char *p,char *out){strcpy(out,canonical(p).c_str());return true;}
static bool DOS_GetSFNPath(const char *p,char *out,bool){strcpy(out,canonical(p).c_str());return true;}
static char *strrchr_dbcs(char *p,char c){return strrchr(p,c);}
static localDrive &getdrive(const std::string &p){return *Drives[toupper(p[0])-'A'];}
static bool DOS_GetFileAttr(const char *p,uint16_t *v){std::string s=canonical(p);return getdrive(s).GetFileAttr(s.c_str()+3,v);}
static bool DOS_SetFileAttr(const char *p,uint16_t v){std::string s=canonical(p);return getdrive(s).SetFileAttr(s.c_str()+3,v);}
static bool DOS_FindFirst(const char *p,uint16_t mask,bool=false){
    std::string s=canonical(p);auto pos=s.find_last_of('\\');DOS_DTA d(dos.dta());
    d.state().pattern=s.substr(pos+1);d.state().search=uint8_t(mask);
    return getdrive(s).FindFirst(s.substr(3,pos-2).c_str(),d);
}
static bool DOS_FindNext(){DOS_DTA d(dos.dta());return Drives[4]->FindNext(d);}
struct DtaResult{char name[13],lname[256];uint32_t size,hsize;uint16_t date,time;uint8_t attr;};
static std::vector<std::string> adirs;
static const char *MSG_Get(const char *s){return s;}
class DOS_Shell {
public:
    std::string output;
    void CMD_ATTRIB(char*);
    void WriteOut(const char *fmt,...){char buf[2048];va_list ap;va_start(ap,fmt);vsnprintf(buf,sizeof(buf),fmt,ap);va_end(ap);output+=buf;}
    void WriteOut_NoParsing(const char *p,bool){output+=p;}
};
static bool CheckBreak(DOS_Shell*){return false;}
static char *trim(char *p){while(*p==' ')++p;size_t n=strlen(p);while(n&&p[n-1]==' ')p[--n]=0;return p;}
static void StripSpaces(char *p){memmove(p,trim(p),strlen(trim(p))+1);}
static bool ScanCMDBool(char *p,const char *option){
    for(size_t i=0;p[i];++i)if((!i||p[i-1]==' ')&&p[i]=='/'&&!strncasecmp(p+i+1,option,strlen(option))&&(!p[i+1+strlen(option)]||p[i+1+strlen(option)]==' ')){
        memset(p+i,' ',strlen(option)+1);return true;
    }
    return false;
}
static char *ScanCMDRemain(char *p){return strchr(p,'/');}
static char *StripArg(char *&p){static char token[CROSS_LEN];p=trim(p);size_t n=strcspn(p," ");memcpy(token,p,n);token[n]=0;p+=n;p=trim(p);return token;}
static char *ExpandDot(char *p,char*,size_t,bool){return p;}
#include "methods.h"
static void attrib(DOS_Shell &shell,const char *command){char args[CROSS_LEN];strcpy(args,command);shell.output.clear();shell.CMD_ATTRIB(args);}
static void create(localDrive &drive,const char *name,uint16_t a=0){DOS_File *f=nullptr;assert(drive.FileCreate(&f,name,a));assert(f);delete f;}
int main(int argc,char **argv){
    assert(argc==2);std::string folder=std::string(argv[1])+"/volume";fs::create_directory(folder);fs::create_directory(folder+"/SUB");
    localDrive drive(folder);Drives[2]=Drives[4]=&drive;
    create(drive,"ONE.TXT");create(drive,"SUB\\TWO.TXT");create(drive,"FLAGS.TXT",7);
    struct stat parent={};assert(!stat(argv[1],&parent));non_fat_parent=parent.st_ino;
    for(const bool read_only : {false,true}) for(const char *root_path : {"","/","\\"}) {
        drive.readonly=read_only;
        DOS_DTA listing(7);listing.state().search=0x37;
        std::vector<std::string> names;
        if(drive.FindFirst(root_path,listing))do{names.push_back(listing.state().name);}while(drive.FindNext(listing));
        assert(parent_attribute_requests==0 && "root listing queried FAT attributes on the Linux parent");
        assert(std::find(names.begin(),names.end(),"ONE.TXT")!=names.end());
        assert(std::find(names.begin(),names.end(),"SUB")!=names.end());
        assert(std::find(names.begin(),names.end(),".")==names.end());
        assert(std::find(names.begin(),names.end(),"..")==names.end());
    }
    drive.readonly=false;
    DOS_DTA sublisting(8);sublisting.state().search=0x37;
    std::vector<std::string> subnames;
    assert(drive.FindFirst("SUB\\",sublisting));
    do{subnames.push_back(sublisting.state().name);}while(drive.FindNext(sublisting));
    assert(std::find(subnames.begin(),subnames.end(),".")!=subnames.end());
    assert(std::find(subnames.begin(),subnames.end(),"..")!=subnames.end());
    assert(std::find(subnames.begin(),subnames.end(),"TWO.TXT")!=subnames.end());
    assert(parent_attribute_requests==0);
    uint16_t a=0;assert(drive.GetFileAttr("ONE.TXT",&a)&&a==DOS_ATTR_ARCHIVE);
    assert(drive.GetFileAttr("FLAGS.TXT",&a)&&a==39);
    DOS_Shell shell;
    attrib(shell,"+R +H +S -A E:\\ONE.TXT");assert(!dos.return_code);assert(drive.GetFileAttr("ONE.TXT",&a)&&a==7);
    assert(shell.output.find("SHR")!=std::string::npos);
    DOS_File *file=nullptr;assert(drive.FileOpen(&file,"ONE.TXT",OPEN_READ)&&file->attr==7);delete file;file=nullptr;
    assert(!drive.FileOpen(&file,"ONE.TXT",OPEN_READWRITE)&&error==DOSERR_ACCESS_DENIED);
    assert(!drive.FileCreate(&file,"ONE.TXT",0)&&error==DOSERR_ACCESS_DENIED);
    assert(!drive.FileUnlink("ONE.TXT")&&error==DOSERR_ACCESS_DENIED);
    assert(fs::exists(folder+"/ONE.TXT"));
    attrib(shell,"E:\\ONE.TXT");assert(shell.output.find("SHR")!=std::string::npos);
    // A new adapter sees the same actual bits; no companion file supplies them.
    localDrive reopened(folder);assert(reopened.GetFileAttr("ONE.TXT",&a)&&a==7);
    DOS_DTA d(0);d.state().pattern="*.*";d.state().search=0;
    assert(!drive.FindFirst("",d)); // Only H/S file and a directory are present.
    d.state().search=0x37;assert(drive.FindFirst("",d));
    bool found=false;do{if(d.state().name=="ONE.TXT"){assert(d.state().attr==7);found=true;}}while(drive.FindNext(d));assert(found);
    attrib(shell,"-R -H -S +A E:\\ONE.TXT");assert(!dos.return_code&&drive.GetFileAttr("ONE.TXT",&a)&&a==32);
    assert(drive.FileOpen(&file,"ONE.TXT",OPEN_READWRITE));
    DOS_File *peer=nullptr;assert(drive.FileOpen(&peer,"ONE.TXT",OPEN_READWRITE));
    auto first_stream=static_cast<LocalFile*>(file)->fhandle;
    auto peer_stream=static_cast<LocalFile*>(peer)->fhandle;
    assert(fwrite("A",1,1,first_stream)==1);
    char byte=0;assert(fread(&byte,1,1,peer_stream)==1&&byte=='A');
    assert(!fseek(first_stream,0,SEEK_SET)&&fwrite("B",1,1,first_stream)==1);
    assert(!fseek(peer_stream,0,SEEK_SET)&&fread(&byte,1,1,peer_stream)==1&&byte=='B');
    delete peer;delete file;
    attrib(shell,"-A E:\\*.TXT /S");assert(!dos.return_code);
    assert(drive.GetFileAttr("ONE.TXT",&a)&&a==0);assert(drive.GetFileAttr("SUB\\TWO.TXT",&a)&&a==0);
    attrib(shell,"+H +S E:\\SUB");assert(!dos.return_code);assert(drive.GetFileAttr("SUB",&a)&&a==22);
    attrib(shell,"+A E:\\*.TXT /S");assert(!dos.return_code);assert(drive.GetFileAttr("SUB\\TWO.TXT",&a)&&a==32);
    attrib(shell,"+R C:\\ONE.TXT");assert(!dos.return_code&&drive.GetFileAttr("ONE.TXT",&a)&&a==33);
    attrib(shell,"-R C:\\ONE.TXT");assert(!dos.return_code);
    fail_set=true;attrib(shell,"+H E:\\ONE.TXT");assert(dos.return_code==1&&shell.output.find("SET_ERROR")!=std::string::npos);fail_set=false;
    fail_sync=true;attrib(shell,"-H E:\\ONE.TXT");assert(dos.return_code==1);fail_sync=false;
    fail_get=true;assert(!drive.GetFileAttr("ONE.TXT",&a)&&error==29);fail_get=false;
    drive.readonly=true;attrib(shell,"+S E:\\ONE.TXT");assert(dos.return_code==1);
    assert(!drive.SetFileAttr("ONE.TXT",4)&&error==DOSERR_WRITE_PROTECTED);drive.readonly=false;
    attrib(shell,"E:\\MISSING.TXT");assert(dos.return_code==1);
    assert(drive.SetFileAttr("SUB",0xffff));assert(drive.GetFileAttr("SUB",&a)&&a==0x37); // D is preserved, V cannot be set.
    assert(!drive.SetFileAttr("",0));
    assert(sync_count>0);attrib(shell,"-R -H -S E:\\*.TXT");assert(drive.FileUnlink("ONE.TXT"));assert(drive.FileUnlink("FLAGS.TXT"));
    for(const auto &entry:fs::directory_iterator(folder))assert(entry.path().filename()=="SUB");
    puts("PASS: native volume root/subdirectory listings, R/H/S/A, create/open/delete protection, persistence, flush errors and ATTRIB combinations/recursion/errorlevel");
}
