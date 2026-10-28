#!/usr/bin/env python3
"""Exercise the patched upstream FAT file methods against an in-memory disk.

Pass a patched DOSBox-X runtime source directory, or use the current ARM64 build.
"""
from pathlib import Path
import argparse
import subprocess
import tempfile

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('runtime', type=Path, nargs='?')
args = parser.parse_args()
root = Path(__file__).resolve().parents[2]
runtime = args.runtime or next((root / 'output/buildroot-arm64/build').glob('pi-dos-runtime-*'), None)
if runtime is None:
    parser.error('a patched runtime source directory is required; build the runtime first')
source = (runtime / 'src/dos/drive_fat.cpp').read_text()
if 'usbSectorDirty' not in source or 'direntry encoded = {};' not in source:
    parser.error('runtime sources need 0015-usb-floppy-fat-io.patch and 0016-usb-floppy-metadata.patch')
start=source.index('class fatFile :')
parts=[source[start:source.index('\n};',start)+3]]
for signature in ['fatFile::fatFile(', 'void fatFile::Flush(', 'bool fatFile::Read(', 'bool fatFile::Write(', 'bool fatFile::Seek(', 'bool fatFile::Close(', 'uint16_t fatFile::GetInformation(', 'bool fatFile::UpdateDateTimeFromHost(', 'uint32_t fatFile::GetSeekPos(']:
    start=source.index(signature)
    parts.append(source[start:source.index('\n}',start)+2])
for signature in ['static void copyDirEntry(', 'bool fatDrive::directoryChange(', 'void fatDrive::UpdateDPB(']:
    start=source.index(signature)
    parts.append(source[start:source.index('\n}',start)+2]
                 .replace('fatDrive::directoryChange(', 'DirectoryDrive::directoryChange(')
                 .replace('fatDrive::UpdateDPB(', 'DPBDrive::UpdateDPB('))
prefix=r'''
#include <algorithm>
#include <cassert>
#include <cstdint>
#include <cstring>
#include <ctime>
#include <cstdio>
#define SECTOR_SIZE_MAX 512
static void fake_log(const char*,...) {}
#define LOG(...) fake_log
#define LOG_MSG(...) ((void)0)
#define DOS_SEEK_SET 0
#define DOS_SEEK_CUR 1
#define DOS_SEEK_END 2
#define OPEN_READ 0
#define OPEN_WRITE 1
#define DOSERR_ACCESS_DENIED 5
static void DOS_SetError(int) {}
static void time_t_to_DOS_DateTime(uint16_t &t,uint16_t &d,time_t) {t=1;d=2;}
class DOS_File {
public:
 uint32_t flags=2; bool open=true,newtime=false; uint16_t time=0,date=0;
 virtual bool Read(uint8_t*,uint16_t*)=0;
 virtual bool Write(const uint8_t*,uint16_t*)=0;
 virtual bool Seek(uint32_t*,uint32_t)=0;
 virtual bool Close()=0;
 virtual uint16_t GetInformation()=0;
 virtual void Flush()=0;
 virtual bool UpdateDateTimeFromHost()=0;
 virtual uint32_t GetSeekPos()=0;
};
struct __attribute__((packed)) direntry {
 uint8_t entryname[11],attrib,NTRes,milliSecondStamp;
 uint16_t crtTime,crtDate,accessDate,hiFirstClust,modTime,modDate,loFirstClust;
 uint32_t entrysize;
 void SetCluster32(uint32_t v) {loFirstClust=v;hiFirstClust=v>>16;}
};
static_assert(sizeof(direntry)==32,"DOS directory entry size");
template<class T> static void var_write(T *p,T v){memcpy(p,&v,sizeof(v));}
class fatDrive {
public:
 struct clusterChainMemory {};
 void *loadedDisk=this;
 bool native=true;
 uint8_t media[100][512]={};
 unsigned int reads=0,writes=0,allocated=0;
 direntry entry={};
 fatDrive() {memset(media,0xa5,sizeof(media));}
 unsigned int getSectorSize(){return 512;}
 unsigned int getClusterSize(){return 512;}
 uint32_t getFirstFreeClust(){return 2;}
 void allocateCluster(uint32_t,uint32_t){allocated=1;}
 uint32_t appendCluster(uint32_t,clusterChainMemory * = nullptr,clusterChainMemory * = nullptr){++allocated;return 2;}
 void deleteClustChain(uint32_t,uint32_t pos){allocated=(pos+511)/512;}
 uint32_t getAbsoluteSectFromBytePos(uint32_t,uint32_t pos,clusterChainMemory*){return pos/512<allocated?pos/512+1:0;}
 void readSector(uint32_t sector,void*data){assert(sector&&sector<100);++reads;memcpy(data,media[sector],512);}
 void writeSector(uint32_t sector,void*data){assert(sector&&sector<100);++writes;memcpy(media[sector],data,512);}
 void directoryBrowse(uint32_t,direntry*e,int32_t){*e=entry;}
 void directoryChange(uint32_t,const direntry*e,int32_t){entry=*e;}
 struct BPB {bool is_fat32(){return false;}};
 BPB GetBPB(){return {};}
};
static bool PI_DOS_IsUSBFloppyImage(void*p){return static_cast<fatDrive*>(p)->native;}
// Exercise the actual directory method separately, including subdirectories.
#define MAX_DIRENTS_PER_SECTOR 16
class DirectoryDrive {
public:
 fatDrive backing;
 void *loadedDisk=&backing;
 bool unformatted=false;
 uint32_t firstRootDirSect=19;
 struct {
  struct {unsigned int BPB_RootEntCnt=224;} v;
  bool is_fat32(){return false;}
 } BPB;
 unsigned int getSectSize(){return 512;}
 uint32_t getAbsoluteSectFromChain(uint32_t,unsigned int n){return 35+n;}
 void readSector(uint32_t n,void *p){backing.readSector(n,p);}
 void writeSector(uint32_t n,void *p){backing.writeSector(n,p);}
 bool directoryChange(uint32_t,const direntry*,int32_t);
};
using PhysPt=uint32_t;
static unsigned char dpb_memory[2048]={};
static PhysPt DOS_Get_DPB(unsigned int drive){return drive<26?64+drive*64:0;}
static void mem_writeb(PhysPt p,uint8_t v){assert(p<sizeof(dpb_memory));dpb_memory[p]=v;}
static void mem_writew(PhysPt p,uint16_t v){mem_writeb(p,v);mem_writeb(p+1,v>>8);}
static void mem_writed(PhysPt p,uint32_t v){mem_writew(p,v);mem_writew(p+2,v>>16);}
static uint16_t mem_readw(PhysPt p){return dpb_memory[p]|(uint16_t(dpb_memory[p+1])<<8);}
static struct {struct {unsigned int major=5;} version;} dos;
namespace bitop {static unsigned int log2(unsigned int v){unsigned int n=0;while(v>1){v>>=1;++n;}return n;}}
class DPBDrive {
public:
 struct {
  struct {
   uint16_t BPB_BytsPerSec=512,BPB_RsvdSecCnt=1,BPB_RootEntCnt=224,BPB_FATSz16=9;
   uint8_t BPB_SecPerClus=1,BPB_NumFATs=2;
  } v;
  bool is_fat32(){return false;}
 } BPB;
 uint32_t firstDataSector=33,firstRootDirSect=19,partSectOff=0,CountOfClusters=2847;
 uint8_t GetMediaByte(){return BPB.v.BPB_SecPerClus==1?0xf0:0xf9;}
 void UpdateDPB(unsigned char);
};
'''
tests=r'''
int main(){
 fatDrive drive;
 fatFile file("",0,0,&drive);
 uint8_t bytes[1536];for(unsigned int i=0;i<sizeof(bytes);++i)bytes[i]=i/512+1;
 uint16_t size=sizeof(bytes);
 assert(file.Write(bytes,&size)&&size==1536&&drive.reads==0&&drive.writes==3);
 assert(drive.media[1][0]==1&&drive.media[2][0]==2&&drive.media[3][511]==3);
 unsigned int before=drive.writes;
 file.Flush(); file.Close();assert(drive.writes==before);
 // Consecutive full-sector overwrites inside an existing file.
 uint32_t pos=0;file.Seek(&pos,DOS_SEEK_SET);
 memset(bytes,0x55,512);size=512;assert(file.Write(bytes,&size));
 memset(bytes,0x66,512);size=512;assert(file.Write(bytes,&size));
 assert(drive.media[1][511]==0x55&&drive.media[2][0]==0x66&&drive.media[3][0]==3);
 // A partial overwrite preserves untouched bytes.
 pos=520;file.Seek(&pos,DOS_SEEK_SET);size=5;memset(bytes,0x77,5);
 assert(file.Write(bytes,&size));
 assert(drive.media[2][7]==0x66&&drive.media[2][8]==0x77&&drive.media[2][13]==0x66);
 before=drive.writes;file.Flush();file.Close();assert(drive.writes==before);
 // Stop at a requested boundary; load on demand on the next Read/Write.
 pos=0;file.Seek(&pos,DOS_SEEK_SET);before=drive.reads;
 size=512;assert(file.Read(bytes,&size)&&drive.reads==before);
 assert(bytes[0]==0x55&&!file.loadedSector);
 size=512;assert(file.Read(bytes,&size)&&drive.reads==before+1&&bytes[8]==0x77);
 size=512;memset(bytes,0x88,512);assert(file.Write(bytes,&size));
 assert(drive.media[3][511]==0x88);
 pos=0;file.Seek(&pos,DOS_SEEK_SET);size=1536;before=drive.reads;
 assert(file.Read(bytes,&size)&&size==1536&&drive.reads==before+2);
 size=1;assert(file.Read(bytes,&size)&&size==0&&drive.reads==before+2);
 // Partial new writes and consecutive appends retain all bytes.
 fatDrive short_drive;fatFile short_file("",0,0,&short_drive);
 size=5;memset(bytes,0x11,5);assert(short_file.Write(bytes,&size));
 size=7;memset(bytes,0x22,7);assert(short_file.Write(bytes,&size));
 assert(short_drive.media[1][4]==0x11&&short_drive.media[1][5]==0x22&&short_drive.media[1][12]==0xa5);
 before=short_drive.writes;short_file.Close();assert(short_drive.writes==before);
 // Identical root/subdirectory entries cause no write; every changed field
 // still reaches the disk. Compare encoded bytes and preserve neighbours.
 DirectoryDrive directory;
 direntry entry={};entry.entryname[0]='F';entry.entrysize=512;entry.loFirstClust=2;
 unsigned char expected[512];memcpy(expected,directory.backing.media[19],512);
 copyDirEntry(&entry,reinterpret_cast<direntry*>(expected+3*32));
 memcpy(directory.backing.media[19],expected,512);
 assert(directory.directoryChange(0,&entry,3)&&directory.backing.writes==0);
 entry.entrysize=1024;
 assert(directory.directoryChange(0,&entry,3)&&directory.backing.writes==1);
 assert(directory.directoryChange(0,&entry,3)&&directory.backing.writes==1);
 entry.modTime=123;entry.modDate=456;
 assert(directory.directoryChange(0,&entry,3)&&directory.backing.writes==2);
 entry.attrib=0x21;
 assert(directory.directoryChange(0,&entry,3)&&directory.backing.writes==3);
 entry.loFirstClust=3;
 assert(directory.directoryChange(0,&entry,3)&&directory.backing.writes==4);
 copyDirEntry(&entry,reinterpret_cast<direntry*>(expected+3*32));
 assert(!memcmp(directory.backing.media[19],expected,512));
 memcpy(directory.backing.media[35],expected,512);
 assert(directory.directoryChange(2,&entry,3)&&directory.backing.writes==4);
 entry.accessDate=789;
 assert(directory.directoryChange(2,&entry,3)&&directory.backing.writes==5);
 directory.backing.native=false;
 assert(directory.directoryChange(2,&entry,3)&&directory.backing.writes==6);
 directory.backing.media[35][3*32]=0;
 assert(!directory.directoryChange(2,&entry,3)&&directory.backing.writes==6);
 // Actual FAT DPB serialization: DOS 4+ and DOS 3.x layouts, including a
 // 720-KiB replacement disk with two sectors per cluster and different FATs.
 DPBDrive dpb;const PhysPt ptr=DOS_Get_DPB(0);
 dpb_memory[ptr+0x19]=0xab;dpb.UpdateDPB(0);
 assert(mem_readw(ptr+2)==512&&dpb_memory[ptr+4]==0&&dpb_memory[ptr+5]==0);
 assert(mem_readw(ptr+6)==1&&dpb_memory[ptr+8]==2&&mem_readw(ptr+9)==224);
 assert(mem_readw(ptr+0x0b)==33&&mem_readw(ptr+0x0d)==2848);
 assert(mem_readw(ptr+0x0f)==9&&mem_readw(ptr+0x11)==19&&dpb_memory[ptr+0x17]==0xf0);
 assert(mem_readw(ptr+0x1f)==0xffff&&dpb_memory[ptr+0x19]==0xab);
 dpb.BPB.v.BPB_SecPerClus=2;dpb.BPB.v.BPB_RootEntCnt=112;dpb.BPB.v.BPB_FATSz16=3;
 dpb.firstDataSector=14;dpb.firstRootDirSect=7;dpb.CountOfClusters=713;dpb.UpdateDPB(0);
 assert(dpb_memory[ptr+4]==1&&dpb_memory[ptr+5]==1&&mem_readw(ptr+9)==112);
 assert(mem_readw(ptr+0x0b)==14&&mem_readw(ptr+0x0d)==714);
 assert(mem_readw(ptr+0x0f)==3&&mem_readw(ptr+0x11)==7&&dpb_memory[ptr+0x17]==0xf9);
 dos.version.major=3;dpb.UpdateDPB(0);
 assert(dpb_memory[ptr+0x0f]==3&&mem_readw(ptr+0x10)==7&&dpb_memory[ptr+0x16]==0xf9);
 assert(mem_readw(ptr+0x1e)==0xffff);
 puts("PASS: actual FAT methods: full/partial writes, boundaries, EOF, clean flush/close, directory updates and DOS 3/4+ DPB geometry");
}
'''
with tempfile.TemporaryDirectory(prefix='pi-dos-floppy-fat.') as temporary:
    work = Path(temporary)
    check = work / 'check.cpp'
    check.write_text(prefix + '\n'.join(parts) + tests)
    executable = work / 'check'
    subprocess.run(['g++', '-std=c++11', '-DLINUX', '-Wall', '-Wextra', '-Werror',
                    str(check), '-o', str(executable)], check=True)
    subprocess.run([str(executable)], check=True)
