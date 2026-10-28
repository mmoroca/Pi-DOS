/* Included by DOSBox-X dos_programs.cpp after drives.h and bios_disk.h.
 * Original Pi-DOS integration; distributed with the DOSBox-X GPL runtime. */
#ifndef PI_DOS_USB_FLOPPY_DOS_H
#define PI_DOS_USB_FLOPPY_DOS_H
#if defined(LINUX) && !defined(OSFREE)
#include "pidos_usb_floppy_linux.h"
#include <condition_variable>
#include <mutex>
#include <thread>

// Defined by the pinned runtime's callback.cpp but absent from callback.h.
void CALLBACK_IdleNoInts();

namespace pidos_floppy {

/* Only the blocking ioctl runs off-thread. All emulator state and display/
 * timer processing stay on the emulator thread. Reuse one worker, rather
 * than creating a thread for every sector command. */
class IoWorker {
    std::mutex mutex;
    std::condition_variable wake, completed;
    bool stop = false, queued = false, done = false;
    int fd = -1, result = 0, error = 0;
    sg_io_hdr_t *io = nullptr;
    IoTiming *timing = nullptr;
    std::thread worker;
    void work()
    {
        std::unique_lock<std::mutex> lock(mutex);
        for (;;) {
            wake.wait(lock, [this] { return stop || queued; });
            if (stop) return;
            queued = false;
            lock.unlock();
            result = direct_io(fd, io, error, timing);
            lock.lock();
            done = true;
            completed.notify_one();
        }
    }
public:
    IoWorker() : worker([this] { work(); }) {}
    ~IoWorker()
    {
        { std::lock_guard<std::mutex> lock(mutex); stop = true; }
        wake.notify_one(); worker.join();
    }
    int run(int descriptor, sg_io_hdr_t *request, int &saved_errno, IoTiming *measurement)
    {
        std::unique_lock<std::mutex> lock(mutex);
        fd = descriptor; io = request; timing = measurement; done = false; queued = true;
        wake.notify_one();
        const auto old_if = GETFLAG(IF);
        /* Do not allow guest interrupt handlers to reenter DOS/disk code
         * while the request and its stack buffers are owned by the worker. */
        SETFLAGBIT(IF, false);
        try {
            while (!done) {
                lock.unlock();
                CALLBACK_IdleNoInts();
                lock.lock();
                /* The runtime paces its own idle loop and advances video/PIC
                 * events by emulated cycles. Sleeping after each tiny idle
                 * trampoline would starve those events and stall redraws. */
            }
        } catch (...) {
            /* A quit/reset must not free the CDB, sense or transfer buffers
             * until the kernel has finished with them. */
            if (!lock.owns_lock()) lock.lock();
            completed.wait(lock, [this] { return done; });
            SETFLAGBIT(IF, old_if);
            throw;
        }
        SETFLAGBIT(IF, old_if);
        saved_errno = error;
        return result;
    }
};

static int responsive_io(int fd, sg_io_hdr_t *io, int &error, IoTiming *timing)
{ static IoWorker worker; return worker.run(fd, io, error, timing); }

class Disk final : public imageDisk {
    std::shared_ptr<Device> device;
    static Int13Status status(Result result)
    {
        if (result != Result::ok)
            DOS_SetError(result == Result::protected_disk ? DOSERR_WRITE_PROTECTED : DOSERR_DRIVE_NOT_READY);
        switch (result) {
        case Result::ok: return Int13Status::NoError;
        case Result::changed: return Int13Status::DiskChanged;
        case Result::absent: return Int13Status::DriveNotReady;
        case Result::protected_disk: return Int13Status::WriteProtected;
        default: return Int13Status::DataError;
        }
    }
public:
    explicit Disk(std::shared_ptr<Device> source) : imageDisk(ID_BASE), device(source)
    {
        diskname = device->path;
        heads = device->heads; sectors = device->sectors; cylinders = device->cylinders;
        sector_size = 512; image_length = uint64_t(device->blocks) * 512;
        diskSizeK = image_length / 1024; active = true; hardDrive = false;
    }
    Int13Status Read_AbsoluteSector(uint32_t sector, void *data) override
    { memset(data, 0, 512); return status(device->sector(sector, data)); }
    Int13Status Write_AbsoluteSector(uint32_t sector, const void *data) override
    { return status(device->sector(sector, const_cast<void *>(data), true)); }
};

struct Medium {
    std::shared_ptr<Device> device;
    std::unique_ptr<fatDrive> fat;
    explicit Medium(std::shared_ptr<Device> source) : device(source) {}
    bool load_fat()
    {
        if (fat) return true;
        if (!device->fat_geometry()) { DOS_SetError(DOSERR_DRIVE_NOT_READY); return false; }
        std::vector<std::string> options;
        if (device->readonly) options.emplace_back("readonly");
        auto disk = new Disk(device);
        disk->Addref();
        fat.reset(new fatDrive(disk, options));
        disk->Release();
        if (!fat->created_successfully || !device->valid) { fat.reset(); return false; }
        return true;
    }
    bool ready(bool force = false)
    {
        if (device->valid && device->ready(force) == Result::ok) return true;
        device->invalidate();
        DOS_SetError(DOSERR_DRIVE_NOT_READY);
        return false;
    }
};

/* Coalesce only within one DOS operation. Never leave dirty sectors waiting
 * for a later close, timer or unmount; abandon the medium on exceptions. */
class WriteBatch {
    std::shared_ptr<Device> device;
    bool active;
public:
    explicit WriteBatch(std::shared_ptr<Device> source) : device(source), active(device->begin_write()) {}
    ~WriteBatch() { if (active) device->cancel_write(); }
    bool finish()
    {
        if (!active) { DOS_SetError(device->readonly ? DOSERR_WRITE_PROTECTED : DOSERR_DRIVE_NOT_READY); return false; }
        const Result result = device->end_write();
        active = false;
        if (result != Result::ok)
            DOS_SetError(result == Result::protected_disk ? DOSERR_WRITE_PROTECTED : DOSERR_DRIVE_NOT_READY);
        return result == Result::ok;
    }
};

struct Controller {
    std::shared_ptr<Medium> medium;
    bool io_ready(bool writing = false)
    {
        if (writing && medium && medium->fat) medium->device->invalidate();
        if (!medium || !medium->device->valid) return ready(false, writing);
        if (writing && medium->device->readonly) { DOS_SetError(DOSERR_WRITE_PROTECTED); return false; }
        return true; // The imminent READ/WRITE reports unit attention or missing media.
    }
    bool ready(bool files = false, bool writing = false, bool force = false)
    {
        /* Raw writes retire every existing DOS handle, even when formatting
         * the same disk. Its old FAT and directory caches are no longer valid. */
        if (writing && medium && medium->fat) medium->device->invalidate();
        if (!medium || !medium->ready(writing || force)) {
            medium.reset();
            auto device = discover("/sys/class/block", "/dev", false);
            if (!device) { DOS_SetError(DOSERR_DRIVE_NOT_READY); return false; }
            medium = std::make_shared<Medium>(device);
            imageDiskChange[0] = true;
            LOG_MSG("Pi-DOS: USB floppy A: uses %s (%u KiB, %s)", device->path.c_str(),
                    device->blocks / 2, device->readonly ? "read-only" : "read/write");
        }
        if (writing && medium->device->readonly) { DOS_SetError(DOSERR_WRITE_PROTECTED); return false; }
        return !files || medium->load_fat();
    }
    Result transfer(uint32_t sector, void *data, bool write)
    {
        if (!io_ready(write))
            return medium && medium->device->readonly ? Result::protected_disk : Result::absent;
        return medium->device->sector(sector, data, write);
    }
};

class BiosDisk final : public imageDisk {
public:
    std::shared_ptr<Controller> controller;
    explicit BiosDisk(std::shared_ptr<Controller> source) : imageDisk(ID_BASE), controller(source)
    { active = true; heads = 2; sectors = 18; cylinders = 80; sector_size = 512; image_length = 1440 * 1024; diskSizeK = 1440; }
    void Get_Geometry(uint32_t *h, uint32_t *c, uint32_t *s, uint32_t *b) override
    {
        if (controller->ready()) {
            const auto &device = controller->medium->device;
            heads = device->heads; cylinders = device->cylinders; sectors = device->sectors;
            image_length = uint64_t(device->blocks) * 512; diskSizeK = image_length / 1024;
        }
        *h = heads; *c = cylinders; *s = sectors; *b = 512;
    }
    uint8_t GetBiosType() override
    {
        uint32_t h, c, s, b; Get_Geometry(&h, &c, &s, &b);
        return s == 36 ? 5 : s == 18 ? 4 : s == 15 ? 2 : c == 40 ? 1 : 3;
    }
    static Int13Status status(Result result)
    {
        switch (result) {
        case Result::ok: return Int13Status::NoError;
        case Result::protected_disk: return Int13Status::WriteProtected;
        case Result::changed: return Int13Status::DiskChanged;
        case Result::absent: return Int13Status::DriveNotReady;
        default: return Int13Status::DataError;
        }
    }
    Int13Status Read_AbsoluteSector(uint32_t sector, void *data) override
    { return status(controller->transfer(sector, data, false)); }
    Int13Status Write_AbsoluteSector(uint32_t sector, const void *data) override
    { return status(controller->transfer(sector, const_cast<void *>(data), true)); }
    Int13Status chs(uint32_t head, uint32_t cylinder, uint32_t sector, void *data, unsigned int size, bool write)
    {
        uint32_t h, c, s, b; Get_Geometry(&h, &c, &s, &b);
        if ((size && size != 512) || head >= h || cylinder >= c || !sector || sector > s)
            return Int13Status::SectorNotFound;
        return status(controller->transfer((cylinder * h + head) * s + sector - 1, data, write));
    }
    Int13Status Read_Sector(uint32_t h, uint32_t c, uint32_t s, void *data, unsigned int size = 0) override
    { return chs(h, c, s, data, size, false); }
    Int13Status Write_Sector(uint32_t h, uint32_t c, uint32_t s, const void *data, unsigned int size = 0) override
    { return chs(h, c, s, const_cast<void *>(data), size, true); }
};

/* Each DOS handle retains its original FAT driver. Once the device reports a
 * media change it stays invalid, including for Close/Flush: stale handles must
 * never write directory entries or buffered sectors onto a replacement disk. */
class File final : public DOS_File {
    std::shared_ptr<Medium> medium;
    std::unique_ptr<DOS_File> file;
    void to_file()
    { file->time = time; file->date = date; file->newtime = newtime; }
    void from_file()
    { time = file->time; date = file->date; newtime = file->newtime; attr = file->attr; }
    bool dirty() const
    { return newtime || ((flags & 15) != OPEN_READ && (flags & 15) != OPEN_READ_NO_MOD); }
public:
    File(std::shared_ptr<Medium> source, DOS_File *handle) : medium(source), file(handle)
    {
        flags = file->flags; open = true; from_file();
        if (file->GetName()) SetName(file->GetName());
    }
    bool Read(uint8_t *data, uint16_t *size) override
    {
        if (!medium->ready()) { *size = 0; return false; }
        const bool result = file->Read(data, size); from_file();
        return result && medium->device->valid;
    }
    bool Write(const uint8_t *data, uint16_t *size) override
    {
        if (!medium->ready()) { *size = 0; return false; }
        if (medium->device->readonly) { *size = 0; DOS_SetError(DOSERR_WRITE_PROTECTED); return false; }
        WriteBatch batch(medium->device);
        to_file();
        const bool result = file->Write(data, size);
        from_file();
        const bool committed = batch.finish();
        if (!committed) *size = 0;
        return result && committed;
    }
    bool Seek(uint32_t *pos, uint32_t type) override
    { return medium->ready() && file->Seek(pos, type) && medium->device->valid; }
    bool Close() override
    {
        const bool ready = medium->ready(true);
        const bool flush = ready && dirty() && !medium->device->readonly;
        bool committed = true;
        if (flush) {
            WriteBatch batch(medium->device);
            to_file();
            if (refCtr <= 1) file->Close(); else file->Flush();
            from_file();
            committed = batch.finish();
        }
        if (refCtr <= 1) {
            open = false;
        }
        return committed;
    }
    void Flush() override
    {
        if (medium->ready(true) && dirty() && !medium->device->readonly) {
            WriteBatch batch(medium->device);
            to_file(); file->Flush(); from_file(); batch.finish();
        }
    }
    uint16_t GetInformation() override { return file->GetInformation(); }
    uint32_t GetSeekPos() override { return file->GetSeekPos(); }
};

class Drive final : public DOS_Drive {
    std::shared_ptr<Medium> &medium;
    std::weak_ptr<Medium> directory_medium;
    imageDisk *bios = nullptr;
    bool complete(bool result, bool writing = false)
    {
        if (!medium->device->valid) { DOS_SetError(DOSERR_DRIVE_NOT_READY); return false; }
        if (writing && medium->device->readonly) { DOS_SetError(DOSERR_WRITE_PROTECTED); return false; }
        return result;
    }
    bool ready(bool force = false)
    {
        if (!controller->ready(true, false, force)) return false;
        readonly = medium->device->readonly;
        if (directory_medium.lock() != medium) { curdir[0] = 0; directory_medium = medium; }
        return true;
    }
    template<class Operation> bool mutate(Operation operation)
    {
        if (!ready(true)) return false;
        if (readonly) { DOS_SetError(DOSERR_WRITE_PROTECTED); return false; }
        WriteBatch batch(medium->device);
        const bool result = operation();
        const bool committed = batch.finish();
        return committed && complete(result, true);
    }
public:
    std::shared_ptr<Controller> controller;
    Drive(std::shared_ptr<Controller> source) : medium(source->medium), controller(source)
    {
        strcpy(info, "USB floppy (automatic)"); readonly = false;
        bios = new BiosDisk(controller);
        bios->Addref(); imageDiskList[0] = bios;
    }
    ~Drive()
    {
        if (imageDiskList[0] == bios) { imageDiskList[0] = nullptr; bios->Release(); }
    }
    bool FileOpen(DOS_File **file, const char *name, uint32_t flags) override
    {
        if (!ready(true)) return false;
        DOS_File *handle = nullptr;
        const bool result = medium->fat->FileOpen(&handle, name, flags);
        if (!complete(result)) { delete handle; return false; }
        *file = new File(medium, handle); return true;
    }
    bool FileCreate(DOS_File **file, const char *name, uint16_t attr) override
    {
        DOS_File *handle = nullptr;
        if (!mutate([&] { return medium->fat->FileCreate(&handle, name, attr); })) { delete handle; return false; }
        *file = new File(medium, handle); return true;
    }
    bool FileUnlink(const char *name) override { return mutate([&] { return medium->fat->FileUnlink(name); }); }
    bool RemoveDir(const char *name) override { return mutate([&] { return medium->fat->RemoveDir(name); }); }
    bool MakeDir(const char *name) override { return mutate([&] { return medium->fat->MakeDir(name); }); }
    bool TestDir(const char *name) override { return ready(true) && complete(medium->fat->TestDir(name)); }
    bool FindFirst(const char *name, DOS_DTA &dta, bool fcb = false) override
    { return ready(true) && complete(medium->fat->FindFirst(name, dta, fcb)); }
    bool FindNext(DOS_DTA &dta) override
    { return medium && medium->fat && medium->ready() && complete(medium->fat->FindNext(dta)); }
    bool SetFileAttr(const char *name, uint16_t attr) override
    { return mutate([&] { return medium->fat->SetFileAttr(name, attr); }); }
    bool GetFileAttr(const char *name, uint16_t *attr) override
    { return ready() && complete(medium->fat->GetFileAttr(name, attr)); }
    bool GetFileAttrEx(char *name, struct stat *status) override
    { return ready() && complete(medium->fat->GetFileAttrEx(name, status)); }
    unsigned long GetCompressedSize(char *name) override
    { return ready() ? medium->fat->GetCompressedSize(name) : 0; }
    bool Rename(const char *oldname, const char *newname) override
    { return mutate([&] { return medium->fat->Rename(oldname, newname); }); }
    bool AllocationInfo(uint16_t *bytes, uint8_t *cluster, uint16_t *total, uint16_t *free) override
    { return ready() && complete(medium->fat->AllocationInfo(bytes, cluster, total, free)); }
    bool FileExists(const char *name) override { return ready() && complete(medium->fat->FileExists(name)); }
    bool FileStat(const char *name, FileStat_Block *block) override
    { return ready() && complete(medium->fat->FileStat(name, block)); }
    uint8_t GetMediaByte() override { return medium && medium->fat ? medium->fat->GetMediaByte() : 0xf0; }
    bool isRemote() override { return false; }
    bool isRemovable() override { return true; }
    bool RefreshDPB(uint8_t number)
    {
        // Revalidate this medium, never return another disk's cached DPB.
        if (!DOS_Get_DPB(number) || !ready(true)) return false;
        medium->fat->UpdateDPB(number);
        return medium->device->valid;
    }
    char const *GetLabel() override { return ready() ? medium->fat->GetLabel() : ""; }
    void EmptyCache() override { if (medium && medium->fat && medium->ready()) medium->fat->EmptyCache(); }
    uint32_t GetSectorSize() override { return 512; }
    uint32_t GetSectorCount() override { return controller->ready() ? medium->device->blocks : 0; }
    uint8_t Read_AbsoluteSector_INT25(uint32_t sector, void *data) override
    { return controller->transfer(sector, data, false) == Result::ok ? 0 : 5; }
    uint8_t Write_AbsoluteSector_INT25(uint32_t sector, void *data) override
    { return controller->transfer(sector, data, true) == Result::ok ? 0 : 5; }
    Bits UnMount() override { delete this; return 0; }
};

class Install final : public Program {
public:
    void Run() override
    {
        /* Internal appliance startup command. Do not claim A: in native builds
         * or replace an explicit user mount. */
        if (!getenv("PI_DOS_USB_FLOPPY") || strcmp(getenv("PI_DOS_USB_FLOPPY"), "1") || Drives[0] || imageDiskList[0]) return;
        io_executor = responsive_io;
        DriveManager::AppendDisk(0, new Drive(std::make_shared<Controller>()));
        DriveManager::InitializeDrive(0);
        mem_writeb(Real2Phys(dos.tables.mediaid), 0xf0);
        incrementFDD();
        updateFloppyDPT();
    }
};
static void program(Program **make) { *make = new Install; }

} // namespace pidos_floppy

bool PI_DOS_IsUSBFloppy(DOS_Drive *drive)
{ return dynamic_cast<pidos_floppy::Drive *>(drive) != nullptr; }

bool PI_DOS_IsUSBFloppyImage(imageDisk *disk)
{ return dynamic_cast<pidos_floppy::Disk *>(disk) != nullptr; }

bool PI_DOS_USBFloppyDPB(DOS_Drive *drive, uint8_t number)
{
    auto floppy = dynamic_cast<pidos_floppy::Drive *>(drive);
    const bool valid = floppy && floppy->RefreshDPB(number);
    const char *diagnostics = getenv("PI_DOS_DIAGNOSTICS");
    if (diagnostics && !strcmp(diagnostics, "1"))
        fprintf(stderr, "Pi-DOS USB floppy DPB: drive=%u valid=%u\n", unsigned(number), unsigned(valid));
    return valid;
}

bool PI_DOS_USBFloppyBIOS()
{
    using namespace pidos_floppy;
    if (reg_dl != 0) return false;
    auto bios = dynamic_cast<BiosDisk *>(imageDiskList[0]);
    if (!bios || (reg_ah != 2 && reg_ah != 3 && reg_ah != 4 &&
                  reg_ah != 0x05 && reg_ah != 0x17 && reg_ah != 0x18)) return false;
    const unsigned int function = reg_ah;
    auto &controller = *bios->controller;
    Int13Status result = Int13Status::NoError;
    const bool writing = function == 3 || function == 5;
    const bool ready = function <= 5 ? controller.io_ready(writing) : controller.ready();
    if (!ready)
        result = controller.medium && controller.medium->device->readonly ?
                 Int13Status::WriteProtected : Int13Status::DriveNotReady;
    else {
        const auto device = controller.medium->device;
        if (function >= 2 && function <= 4) {
            const unsigned int count = reg_al;
            const unsigned int head = reg_dh, cylinder = reg_ch, sector = reg_cl;
            uint32_t first = (cylinder * device->heads + head) * device->sectors;
            if (!count || cylinder >= device->cylinders || head >= device->heads ||
                !sector || sector > device->sectors ||
                count > device->blocks - first - (sector - 1)) {
                result = Int13Status::SectorNotFound;
            } else if (function == 2 && imageDiskChange[0]) {
                imageDiskChange[0] = false;
                result = Int13Status::DiskChanged;
            } else {
                first += sector - 1;
                unsigned char data[36 * 512];
                unsigned int done = 0;
                while (done < count && result == Int13Status::NoError) {
                    const unsigned int chunk = std::min<unsigned int>(count - done,
                        device->sectors - (first + done) % device->sectors);
                    if (writing) {
                        for (unsigned int i = 0; i < chunk * 512; ++i)
                            data[i] = mem_readb(PhysMake(SegValue(es), static_cast<uint16_t>(reg_bx + done * 512 + i)));
                    }
                    result = BiosDisk::status(device->raw_io(first + done, chunk, data, writing));
                    if (result == Int13Status::NoError && function == 2) {
                        for (unsigned int i = 0; i < chunk * 512; ++i)
                            mem_writeb(PhysMake(SegValue(es), static_cast<uint16_t>(reg_bx + done * 512 + i)), data[i]);
                    }
                    done += chunk;
                }
            }
        } else if (function == 0x18) {
            if (unsigned(reg_ch) + 1 != device->cylinders || reg_cl != device->sectors)
                result = static_cast<Int13Status>(0x0c); // media type not supported
            else {
                updateFloppyDPT();
                const RealPt dpt = RealGetVec(0x1e);
                CPU_SetSegGeneral(es, RealSeg(dpt)); reg_di = RealOff(dpt);
            }
        } else if (function == 5) {
            const unsigned int count = reg_al;
            const PhysPt ids = PhysMake(SegValue(es), reg_bx);
            if (reg_ch >= device->cylinders || reg_dh >= device->heads || count != device->sectors)
                result = Int13Status::SectorNotFound;
            /* Validate the entire sector-ID table before touching the disk. */
            bool seen[37] = {};
            for (unsigned int i = 0; result == Int13Status::NoError && i < count; ++i) {
                const unsigned int sector = mem_readb(ids + i * 4 + 2);
                if (mem_readb(ids + i * 4) != reg_ch || mem_readb(ids + i * 4 + 1) != reg_dh ||
                    mem_readb(ids + i * 4 + 3) != 2 || !sector || sector > count || seen[sector])
                    result = Int13Status::SectorNotFound;
                else seen[sector] = true;
            }
            if (result == Int13Status::NoError) {
                const uint32_t lba = (reg_ch * device->heads + reg_dh) * device->sectors;
                result = BiosDisk::status(device->fill_track(lba, count));
            }
        }
        /* 17h acknowledges the existing physical media type. Unlike the
         * upstream image workaround, it must not force the next read to fail. */
    }
    reg_ah = static_cast<uint8_t>(result);
    CALLBACK_SCF(result != Int13Status::NoError);
    return true;
}

bool PI_DOS_USBFloppyIoctl(DOS_Drive *drive, uint8_t category, uint8_t function, PhysPt ptr, bool query)
{
    using namespace pidos_floppy;
    auto floppy = dynamic_cast<Drive *>(drive);
    if (!floppy || category != 8) { DOS_SetError(DOSERR_FUNCTION_NUMBER_INVALID); return false; }
    switch (function) {
    case 0x40: case 0x60: case 0x41: case 0x61: case 0x42: case 0x62:
    case 0x4a: case 0x4b: case 0x6a: case 0x6b: break;
    default: DOS_SetError(DOSERR_FUNCTION_NUMBER_INVALID); return false;
    }
    if (query || function == 0x4a || function == 0x4b || function == 0x6a || function == 0x6b) return true;
    auto &controller = *floppy->controller;
    const bool writing = function == 0x41 || function == 0x42;
    const bool parameters = function == 0x40 || function == 0x60;
    if (!(parameters ? controller.ready() : controller.io_ready(writing))) return false;
    const auto device = controller.medium->device;
    if (function == 0x60) {
        /* Device parameters must exist even when sector zero has no valid BPB. */
        unsigned char bpb[25] = {};
        auto word = [&bpb](unsigned int at, unsigned int value) { bpb[at] = value; bpb[at + 1] = value >> 8; };
        const unsigned int cluster = device->blocks == 1440 || device->blocks == 5760 ? 2 : 1;
        const unsigned int roots = device->blocks <= 1440 ? 112 : 224;
        unsigned int fats = 1;
        while ((device->blocks - 1 - 2 * fats - roots / 16) / cluster + 2 > fats * 512 * 2 / 3) ++fats;
        word(0, 512); bpb[2] = cluster; word(3, 1); bpb[5] = 2; word(6, roots);
        word(8, device->blocks); bpb[10] = device->blocks >= 2880 ? 0xf0 : 0xf9;
        word(11, fats); word(13, device->sectors); word(15, device->heads);
        mem_writeb(ptr + 1, device->blocks == 1440 ? 2 : device->blocks == 5760 ? 9 : 7);
        mem_writew(ptr + 2, 0); mem_writew(ptr + 4, device->cylinders); mem_writeb(ptr + 6, 0);
        MEM_BlockWrite(ptr + 7, bpb, sizeof(bpb));
        return true;
    }
    if (function == 0x40) {
        if (mem_readw(ptr + 4) != device->cylinders || mem_readw(ptr + 7) != 512 ||
            mem_readw(ptr + 0xf) != device->blocks || mem_readw(ptr + 0x14) != device->sectors ||
            mem_readw(ptr + 0x16) != device->heads) {
            DOS_SetError(DOSERR_FUNCTION_NUMBER_INVALID); return false;
        }
        return true; // Geometry is physical; setting parameters never writes a BPB.
    }
    const unsigned int head = mem_readw(ptr + 1), cylinder = mem_readw(ptr + 3);
    if (head >= device->heads || cylinder >= device->cylinders) {
        DOS_SetError(27); return false; // sector not found
    }
    uint32_t sector = (cylinder * device->heads + head) * device->sectors;
    unsigned int count;
    PhysPt buffer = 0;
    if (function == 0x41 || function == 0x61) {
        const unsigned int offset = mem_readw(ptr + 5);
        count = mem_readw(ptr + 7);
        const uint32_t address = mem_readd(ptr + 9);
        buffer = PhysMake(address >> 16, address & 0xffff);
        if (offset >= device->sectors) { DOS_SetError(27); return false; }
        sector += offset;
    } else {
        const unsigned int tracks = function == 0x62 || (mem_readb(ptr) & 1) ? mem_readw(ptr + 5) : 1;
        count = tracks * device->sectors;
    }
    if (count > device->blocks - sector) { DOS_SetError(27); return false; }
    if (function == 0x42) {
        for (unsigned int i = 0; i < count; i += device->sectors) {
            const Result result = device->fill_track(sector + i, device->sectors);
            if (result != Result::ok) {
                DOS_SetError(result == Result::protected_disk ? DOSERR_WRITE_PROTECTED : 27);
                return false;
            }
        }
        return true;
    }
    unsigned char data[36 * 512];
    for (unsigned int i = 0; i < count;) {
        const unsigned int chunk = std::min<unsigned int>(count - i,
            device->sectors - (sector + i) % device->sectors);
        if (writing) MEM_BlockRead(buffer, data, chunk * 512);
        const Result result = device->raw_io(sector + i, chunk, data, writing);
        if (result != Result::ok) {
            DOS_SetError(result == Result::protected_disk ? DOSERR_WRITE_PROTECTED : 27);
            return false;
        }
        if (function == 0x61) MEM_BlockWrite(buffer, data, chunk * 512);
        buffer += chunk * 512;
        i += chunk;
    }
    return true;
}
#endif
#endif
