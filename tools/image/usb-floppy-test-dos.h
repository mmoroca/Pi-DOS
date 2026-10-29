/* Minimal DOS ABI for exercising the production USB floppy adapter on a host.
 * Guest registers/memory and the upstream FAT driver are stand-ins; USB/SCSI,
 * media lifecycle, BIOS and IOCTL code come from the production headers. */
#include <string>
#include <vector>
#include "dos_sharing.h"
using Bits = intptr_t;
using PhysPt = uint32_t;
using RealPt = uint32_t;
enum { OPEN_READ = 0, OPEN_WRITE = 1, OPEN_READWRITE = 2, OPEN_READ_NO_MOD = 4 };
enum { DOSERR_FUNCTION_NUMBER_INVALID = 1, DOSERR_WRITE_PROTECTED = 19, DOSERR_DRIVE_NOT_READY = 21 };
enum { DOS_ATTR_VOLUME = 8, DOSERR_ACCESS_DENIED = 5 };
static unsigned int dos_error;
static void DOS_SetError(unsigned int error) { dos_error = error; }
struct DOS_DTA {};
struct FileStat_Block {};
class DOS_File {
    std::string name;
public:
    uint16_t time = 0, date = 0, attr = 0;
    uint32_t flags = OPEN_READWRITE;
    bool newtime = false, open = true;
    Bits refCtr = 1;
    virtual ~DOS_File() = default;
    virtual bool Read(uint8_t *, uint16_t *) { return true; }
    virtual bool Write(const uint8_t *, uint16_t *) { return true; }
    virtual bool Seek(uint32_t *, uint32_t) { return true; }
    virtual bool Close() { return true; }
    virtual void Flush() {}
    virtual uint16_t GetInformation() { return 0; }
    virtual uint32_t GetSeekPos() { return 0; }
    virtual bool SharingReady() { return true; }
    virtual bool SharingRefresh() { return true; }
    const char *GetName() { return name.c_str(); }
    void SetName(const char *value) { name = value; }
};
class DOS_Drive {
public:
    char info[256] = {}, curdir[256] = {};
    bool readonly = false;
    virtual ~DOS_Drive() = default;
    uint64_t sharing_media = pidos_sharing::next_media_id();
    virtual uint64_t SharingMediaId() { return sharing_media; }
#define DOS_TEST_METHOD(name, args) virtual bool name args { return false; }
    DOS_TEST_METHOD(FileOpen, (DOS_File **, const char *, uint32_t))
    DOS_TEST_METHOD(FileCreate, (DOS_File **, const char *, uint16_t))
    DOS_TEST_METHOD(FileUnlink, (const char *))
    DOS_TEST_METHOD(RemoveDir, (const char *))
    DOS_TEST_METHOD(MakeDir, (const char *))
    DOS_TEST_METHOD(TestDir, (const char *))
    DOS_TEST_METHOD(FindFirst, (const char *, DOS_DTA &, bool = false))
    DOS_TEST_METHOD(FindNext, (DOS_DTA &))
    DOS_TEST_METHOD(SetFileAttr, (const char *, uint16_t))
    DOS_TEST_METHOD(GetFileAttr, (const char *, uint16_t *))
    DOS_TEST_METHOD(GetFileAttrEx, (char *, struct stat *))
    DOS_TEST_METHOD(Rename, (const char *, const char *))
    DOS_TEST_METHOD(AllocationInfo, (uint16_t *, uint8_t *, uint16_t *, uint16_t *))
    DOS_TEST_METHOD(FileExists, (const char *))
    DOS_TEST_METHOD(FileStat, (const char *, FileStat_Block *))
#undef DOS_TEST_METHOD
    virtual unsigned long GetCompressedSize(char *) { return 0; }
    virtual uint8_t GetMediaByte() { return 0xf0; }
    virtual bool isRemote() { return false; }
    virtual bool isRemovable() { return false; }
    virtual void UpdateDPB(unsigned char) {}
    virtual Bits UnMount() { return 0; }
    virtual const char *GetLabel() { return ""; }
    virtual void SetLabel(const char *, bool, bool) {}
    virtual bool SetLabelChecked(const char *) { return true; }
    virtual void EmptyCache() {}
    virtual uint32_t GetSectorSize() { return 512; }
    virtual uint32_t GetSectorCount() { return 0; }
    virtual uint8_t Read_AbsoluteSector_INT25(uint32_t, void *) { return 5; }
    virtual uint8_t Write_AbsoluteSector_INT25(uint32_t, void *) { return 5; }
};
enum class Int13Status : uint8_t {
    NoError = 0, WriteProtected = 3, SectorNotFound = 4, DiskChanged = 6,
    DataError = 0x10, DriveNotReady = 0xaa,
};
class imageDisk {
    unsigned int refs = 0;
public:
    enum IMAGE_TYPE { ID_BASE };
    explicit imageDisk(IMAGE_TYPE) {}
    virtual ~imageDisk() = default;
    std::string diskname;
    uint32_t heads = 0, sectors = 0, cylinders = 0, sector_size = 512;
    uint64_t image_length = 0, diskSizeK = 0;
    bool active = false, hardDrive = false;
    void Addref() { ++refs; }
    void Release() { assert(refs); if (!--refs) delete this; }
    virtual Int13Status Read_AbsoluteSector(uint32_t, void *) { return Int13Status::DataError; }
    virtual Int13Status Write_AbsoluteSector(uint32_t, const void *) { return Int13Status::DataError; }
    virtual Int13Status Read_Sector(uint32_t, uint32_t, uint32_t, void *, unsigned int = 0) { return Int13Status::DataError; }
    virtual Int13Status Write_Sector(uint32_t, uint32_t, uint32_t, const void *, unsigned int = 0) { return Int13Status::DataError; }
    virtual void Get_Geometry(uint32_t *, uint32_t *, uint32_t *, uint32_t *) {}
    virtual uint8_t GetBiosType() { return 4; }
};
static unsigned int dpb_updates = 0;
static uint8_t dpb_drive = 255;
class fatDrive : public DOS_Drive {
    imageDisk *disk;
    std::string label;
public:
    bool created_successfully = true;
    fatDrive(imageDisk *source, std::vector<std::string> &) : disk(source) { disk->Addref(); }
    ~fatDrive() { disk->Release(); }
    void UpdateDPB(unsigned char number) override { ++dpb_updates; dpb_drive = number; }
    const char *GetLabel() override { return label.c_str(); }
    void SetLabel(const char *value, bool, bool) override {
        unsigned char sector[512] = {};
        label = value;
        memcpy(sector, value, strlen(value));
        disk->Write_AbsoluteSector(19, sector);
    }
};
static imageDisk *imageDiskList[2] = {};
static bool imageDiskChange[2] = {};
static DOS_Drive *Drives[26] = {};
struct DriveManager {
    static void AppendDisk(int n, DOS_Drive *drive) { Drives[n] = drive; }
    static void InitializeDrive(int) {}
};
class Program { public: virtual ~Program() = default; virtual void Run() {} };
static void incrementFDD() {}
static void updateFloppyDPT() {}
#define LOG_MSG(...) ((void)0)
static unsigned char guest[1024 * 1024] = {};
static uint8_t reg_ah, reg_al, reg_ch, reg_cl, reg_dh, reg_dl;
static uint16_t reg_di, reg_bx, es, ds;
static bool carry;
static bool interrupt_flag = true, idle_throws = false;
static unsigned int idle_calls = 0;
#define GETFLAG(flag) interrupt_flag
#define SETFLAGBIT(flag, value) (interrupt_flag = (value))
static void CALLBACK_IdleNoInts()
{ assert(!interrupt_flag); ++idle_calls; if (idle_throws) throw 123; }
static void CALLBACK_SCF(bool value) { carry = value; }
static uint16_t SegValue(uint16_t segment) { return segment; }
static void CPU_SetSegGeneral(uint16_t &segment, uint16_t value) { segment = value; }
static PhysPt PhysMake(uint16_t seg, uint16_t off) { return (uint32_t(seg) << 4) + off; }
static uint16_t RealSeg(RealPt ptr) { return ptr >> 16; }
static uint16_t RealOff(RealPt ptr) { return ptr; }
static RealPt RealGetVec(int) { return 0x1000000; }
static PhysPt Real2Phys(RealPt ptr) { return PhysMake(RealSeg(ptr), RealOff(ptr)); }
static uint8_t mem_readb(PhysPt ptr) { assert(ptr < sizeof(guest)); return guest[ptr]; }
static uint16_t mem_readw(PhysPt ptr) { return mem_readb(ptr) | (uint16_t(mem_readb(ptr + 1)) << 8); }
static uint32_t mem_readd(PhysPt ptr) { return mem_readw(ptr) | (uint32_t(mem_readw(ptr + 2)) << 16); }
static void mem_writeb(PhysPt ptr, uint8_t value) { assert(ptr < sizeof(guest)); guest[ptr] = value; }
static void mem_writew(PhysPt ptr, uint16_t value) { mem_writeb(ptr, value); mem_writeb(ptr + 1, value >> 8); }
static void mem_writed(PhysPt ptr, uint32_t value) { mem_writew(ptr, value); mem_writew(ptr + 2, value >> 16); }
static void MEM_BlockWrite(PhysPt ptr, const void *data, size_t size) { assert(ptr + size <= sizeof(guest)); memcpy(guest + ptr, data, size); }
static void MEM_BlockRead(PhysPt ptr, void *data, size_t size) { assert(ptr + size <= sizeof(guest)); memcpy(data, guest + ptr, size); }
static struct { struct { RealPt mediaid = 0x2000000; uint16_t dpb = 0x300, dpb_size = 0x21; } tables; } dos;
static PhysPt DOS_Get_DPB(unsigned int number)
{ return number < 26 ? PhysMake(dos.tables.dpb, number * dos.tables.dpb_size) : 0; }
