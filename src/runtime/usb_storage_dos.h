/* Included by DOSBox-X dos_programs.cpp. GPL-2.0-or-later. */
#ifndef PI_DOS_USB_STORAGE_DOS_H
#define PI_DOS_USB_STORAGE_DOS_H
#include "pidos_usb_storage_api.h"
#if defined(LINUX) && !defined(OSFREE)
#include "pidos_usb_storage_linux.h"
extern int lfn_filefind_handle;
void CALLBACK_IdleNoInts();

namespace pidos_storage {
static int responsive_io(HostOperation operation)
{
    // Kernel teardown can release drives after DOS callbacks have gone away.
    if (dos_kernel_disabled) return operation();
    static IoWorker worker;
    const auto old_if = GETFLAG(IF);
    SETFLAGBIT(IF, false);
    try {
        const int result = worker.run(std::move(operation), [] { CALLBACK_IdleNoInts(); });
        SETFLAGBIT(IF, old_if);
        return result;
    } catch (...) { SETFLAGBIT(IF, old_if); throw; }
}

inline bool unavailable()
{ DOS_SetError(DOSERR_DRIVE_NOT_READY); return false; }
inline bool io_error()
{
    // DOS extended errors: 112 disk full, 29 write fault.
    DOS_SetError(errno == ENOSPC ? 112 : (errno == EROFS ? DOSERR_WRITE_PROTECTED : 29));
    return false;
}

/* Each open file owns its original volume, even after the letter is reused.
 * Unbuffered stdio avoids read-ahead from a removed medium; every successful
 * DOS write is followed by a checked fsync, including zero-length truncates. */
class File final : public LocalFile {
    std::shared_ptr<Volume> volume;
    bool dirty = false, failed = false;
    bool ready() { return volume->present() || unavailable(); }
    bool commit()
    {
        if (!ready()) return false;
        if (failed) { errno = EIO; return io_error(); }
        if (host_io([this] { return fflush(fhandle) || (dirty && fsync(fileno(fhandle))) ? -1 : 0; })) {
            failed = true;
            return io_error();
        }
        return true;
    }
public:
    File(LocalFile *source, std::shared_ptr<Volume> mounted, bool created)
        : LocalFile(source->name, source->fhandle), volume(mounted), dirty(created)
    {
        flags = source->flags; attr = source->attr;
        time = source->time; date = source->date;
        setvbuf(fhandle, nullptr, _IONBF, 0);
        if (volume->readonly) FlagReadOnlyMedium();
        source->fhandle = nullptr;
        delete source;
    }
    ~File() override
    {
        // Writes are already committed. Destruction during quit/reset only
        // releases the stream; it must not reenter the emulator's idle loop.
        // IoWorker drains borrowed buffers before an exception can reach here.
        if (fhandle) fclose(fhandle);
    }
    bool Read(uint8_t *data, uint16_t *size) override
    {
        if (!ready()) { *size = 0; return false; }
        if ((flags & 15) == OPEN_WRITE) { DOS_SetError(DOSERR_ACCESS_DENIED); *size = 0; return false; }
        const uint16_t requested = *size;
        uint16_t count = 0;
        const int result = host_io([&] {
            if (fseeko(fhandle, 0, SEEK_CUR)) return -1;
            count = uint16_t(fread(data, 1, requested, fhandle));
            return ferror(fhandle) ? -1 : 0;
        });
        *size = count;
        // Preserve localDrive's disk-activity IRQ behavior used by DOS games.
        if (!IS_PC98_ARCH) {
            const uint8_t mask = IO_Read(0x21);
            if (mask & 4) IO_Write(0x21, mask & 0xfb);
        }
        return !result || io_error();
    }
    bool Write(const uint8_t *data, uint16_t *size) override
    {
        if (!ready()) { *size = 0; return false; }
        if (volume->readonly || (flags & 15) == OPEN_READ || (flags & 15) == OPEN_READ_NO_MOD) {
            DOS_SetError(volume->readonly ? DOSERR_WRITE_PROTECTED : DOSERR_ACCESS_DENIED);
            *size = 0; return false;
        }
        if (failed) { *size = 0; errno = EIO; return io_error(); }
        const uint16_t requested = *size;
        uint16_t count = 0;
        dirty = true;
        const int result = host_io([&] {
            if (fseeko(fhandle, 0, SEEK_CUR)) return -1;
            if (!requested) {
                const auto position = ftello(fhandle);
                if (position < 0 || ftruncate(fileno(fhandle), position)) return -1;
            } else {
                count = uint16_t(fwrite(data, 1, requested, fhandle));
                if (count != requested || ferror(fhandle)) return -1;
            }
            return fflush(fhandle) || fsync(fileno(fhandle)) ? -1 : 0;
        });
        *size = count;
        if (result) { failed = true; return io_error(); }
        return true;
    }
    bool Seek(uint32_t *position, uint32_t type) override
    { return ready() && LocalFile::Seek(position, type); }
    bool LockFile(uint8_t mode, uint32_t position, uint16_t size) override
    { return ready() && LocalFile::LockFile(mode, position, size); }
    uint32_t GetSeekPos() override
    { return ready() ? LocalFile::GetSeekPos() : 0xffffffff; }
    bool UpdateDateTimeFromHost() override
    { return ready() && LocalFile::UpdateDateTimeFromHost(); }
    void Flush() override { if (!commit()) failed = true; }
    bool FlushSucceeded() const override { return !failed; }
    bool Close() override
    {
        bool ok = ready() && commit();
        if (ok && !volume->readonly && (dirty || newtime)) {
            if (!newtime) ok = UpdateLocalDateTime();
            struct tm timestamp = {};
            timestamp.tm_sec = (time & 31) * 2;
            timestamp.tm_min = (time >> 5) & 63;
            timestamp.tm_hour = time >> 11;
            timestamp.tm_mday = std::max(1, int(date & 31));
            timestamp.tm_mon = std::max(0, int((date >> 5) & 15) - 1);
            timestamp.tm_year = (date >> 9) + 80;
            timestamp.tm_isdst = -1;
            struct timespec times[2] = {};
            times[0].tv_sec = times[1].tv_sec = mktime(&timestamp);
            if (!volume->present()) ok = unavailable();
            else if (ok && host_io([&] {
                return futimens(fileno(fhandle), times) || fsync(fileno(fhandle)) ? -1 : 0;
            })) ok = io_error();
        }
        // Release failed/retired handles too, preserving duplicated DOS handles.
        if (refCtr <= 1 && fhandle) {
            FILE *stream = fhandle;
            fhandle = nullptr; open = false;
            if (host_io([stream] { return fclose(stream); }) && ok) ok = io_error();
        }
        if (ok && !volume->flush()) ok = io_error();
        if (!ok) failed = true;
        return ok;
    }
};

class Drive;
static Drive *managed[DOS_DRIVES] = {};
static std::set<std::string> ejected;
static uint32_t lfn_epochs[LFN_FILEFIND_MAX] = {};
static unsigned long generation = 0;
static bool enabled = false, scanning = false;

class Drive final : public localDrive {
    std::shared_ptr<Volume> volume;
    uint32_t epoch;
    bool ready() { return volume->present() || unavailable(); }
    bool writable()
    {
        if (!ready()) return false;
        if (!readonly) return true;
        DOS_SetError(DOSERR_WRITE_PROTECTED); return false;
    }
    bool flushed(bool success)
    {
        if (!success) return false;
        return volume->flush() || io_error();
    }
    bool wrap(DOS_File **file, bool created)
    {
        if (file && *file) *file = new File(static_cast<LocalFile *>(*file), volume, created);
        if (created && !flushed(true)) {
            if (file && *file) { delete *file; *file = nullptr; }
            return false;
        }
        return true;
    }
public:
    Drive(std::shared_ptr<Volume> mounted, uint32_t serial, std::vector<std::string> &options)
        : localDrive((mounted->path + "/").c_str(), 512, 64, 32765, 32765, 0xf8, options),
          volume(mounted), epoch(serial)
    {
        readonly = volume->readonly;
        dirCache.SetLabel(volume->label.empty() ? "USB" : volume->label.c_str(), false, false);
    }
    ~Drive() override
    {
        for (auto &drive : managed) if (drive == this) drive = nullptr;
        if (volume->present() && !volume->flush())
            LOG_MSG("Pi-DOS USB storage: final flush failed: %s", volume->candidate.device.c_str());
        volume->detach();
    }
    bool present() const { return volume->present(); }
    std::string key() const { return volume->candidate.key(); }
    bool FileOpen(DOS_File **file, const char *name, uint32_t flags) override
    {
        return ready() && localDrive::FileOpen(file, name, flags) && wrap(file, false);
    }
    bool FileCreate(DOS_File **file, const char *name, uint16_t attributes) override
    {
        return writable() && localDrive::FileCreate(file, name, attributes) && wrap(file, true);
    }
    bool FileUnlink(const char *name) override { return writable() && flushed(localDrive::FileUnlink(name)); }
    bool RemoveDir(const char *name) override { return writable() && flushed(localDrive::RemoveDir(name)); }
    bool MakeDir(const char *name) override { return writable() && flushed(localDrive::MakeDir(name)); }
    bool Rename(const char *from, const char *to) override { return writable() && flushed(localDrive::Rename(from, to)); }
    bool SetFileAttr(const char *name, uint16_t attr) override { return writable() && flushed(localDrive::SetFileAttr(name, attr)); }
    bool TestDir(const char *name) override { return ready() && localDrive::TestDir(name); }
    bool FileExists(const char *name) override { return ready() && localDrive::FileExists(name); }
    bool FileStat(const char *name, FileStat_Block *block) override { return ready() && localDrive::FileStat(name, block); }
    bool GetFileAttr(const char *name, uint16_t *attr) override { return ready() && localDrive::GetFileAttr(name, attr); }
    bool GetFileAttrEx(char *name, struct stat *st) override { return ready() && localDrive::GetFileAttrEx(name, st); }
    unsigned long GetCompressedSize(char *name) override { return ready() ? localDrive::GetCompressedSize(name) : 0; }
    bool AllocationInfo(uint16_t *bytes, uint8_t *sectors, uint16_t *total, uint16_t *free) override
    { return ready() && localDrive::AllocationInfo(bytes, sectors, total, free); }
    bool AllocationInfo64(uint32_t *bytes, uint32_t *sectors, uint64_t *total, uint64_t *free) override
    { return ready() && localDrive::AllocationInfo64(bytes, sectors, total, free); }
    bool FindFirst(const char *dir, DOS_DTA &dta, bool fcb = false) override
    {
        if (!ready()) return false;
        dta.SetDirIDCluster(epoch);
        if (lfn_filefind_handle >= 0 && lfn_filefind_handle < LFN_FILEFIND_MAX)
            lfn_epochs[lfn_filefind_handle] = epoch;
        return localDrive::FindFirst(dir, dta, fcb);
    }
    bool FindNext(DOS_DTA &dta) override
    {
        if (!ready()) return false;
        // LFN searches share a DTA but have independent handles. SFN callers
        // can copy their DTA, so store that generation inside the DTA itself.
        const uint32_t search_epoch = lfn_filefind_handle >= 0 && lfn_filefind_handle < LFN_FILEFIND_MAX
            ? lfn_epochs[lfn_filefind_handle] : dta.GetDirIDCluster();
        if (search_epoch != epoch) { DOS_SetError(DOSERR_NO_MORE_FILES); return false; }
        return localDrive::FindNext(dta);
    }
    FILE *GetSystemFilePtr(const char *name, const char *type) override
    {
        // Host streams bypass checked writes and handle retirement: never export them.
        (void)name; (void)type; DOS_SetError(DOSERR_ACCESS_DENIED); return nullptr;
    }
    bool GetSystemFilename(char *host, const char *name) override
    { (void)host; (void)name; DOS_SetError(DOSERR_ACCESS_DENIED); return false; }
    char const *GetLabel() override { return ready() ? localDrive::GetLabel() : ""; }
    bool isRemovable() override { return true; }
    Bits UnMount() override
    {
        // A manual eject is optional. Do not invalidate files which are still
        // open, and do not immediately remount an explicitly ejected volume.
        if (volume.use_count() != 1 || !volume->flush()) return 1;
        close(volume->fd); volume->fd = -1;
        if (umount(volume->path.c_str())) {
            volume->fd = open(volume->path.c_str(), O_RDONLY | O_DIRECTORY | O_CLOEXEC);
            return 1;
        }
        ejected.insert(key());
        volume->active = false;
        rmdir(volume->path.c_str());
        volume->path.clear();
        delete this;
        return 0;
    }
};

static void poll()
{
    if (!enabled || scanning || dos_kernel_disabled) return;
    static auto next = std::chrono::steady_clock::time_point{};
    const auto now = std::chrono::steady_clock::now();
    if (now < next) return;
    next = now + std::chrono::milliseconds(500);
    scanning = true;
    std::set<std::string> occupied;
    for (int i = 4; i < DOS_DRIVES; ++i) {
        auto drive = managed[i];
        if (!drive) continue;
        if (drive->present()) { occupied.insert(drive->key()); continue; }
        LOG_MSG("Pi-DOS USB storage: %c: removed", 'A' + i);
        if (Drives[i] == drive) {
            Drives[i] = nullptr;
            mem_writeb(Real2Phys(dos.tables.mediaid) + i * 2, 0);
        }
        delete drive;
    }
    static std::map<std::string, std::chrono::steady_clock::time_point> retry;
    std::set<std::string> seen;
    for (const auto &candidate : discover()) {
        const auto key = candidate.key();
        seen.insert(key);
        if (ejected.count(key) || occupied.count(key) || (retry.count(key) && now < retry[key])) continue;
        int letter = 4;
        while (letter < DOS_DRIVES && Drives[letter]) ++letter;
        if (letter == DOS_DRIVES) {
            LOG_MSG("Pi-DOS USB storage: no drive letters available for %s", candidate.device.c_str());
            retry[key] = now + std::chrono::seconds(30);
            continue;
        }
        retry[key] = now + std::chrono::seconds(5);
        auto volume = mount_volume(candidate, ++generation);
        if (!volume) continue;
        std::vector<std::string> options;
        auto drive = new Drive(volume, uint32_t(generation), options);
        Drives[letter] = managed[letter] = drive;
        mem_writeb(Real2Phys(dos.tables.mediaid) + letter * 2, 0xf8);
        LOG_MSG("Pi-DOS USB storage: %s -> %c: (%s, %s)", candidate.device.c_str(),
                'A' + letter, volume->filesystem.c_str(), volume->readonly ? "read-only" : "synchronous");
    }
    for (auto at = retry.begin(); at != retry.end(); )
        if (!seen.count(at->first)) at = retry.erase(at); else ++at;
    for (auto at = ejected.begin(); at != ejected.end(); )
        if (!seen.count(*at)) at = ejected.erase(at); else ++at;
    scanning = false;
}

class Install final : public Program {
public:
    void Run() override
    {
        const char *opt = getenv("PI_DOS_USB_STORAGE");
        if (!opt || strcmp(opt, "1") || geteuid() != 0) return;
        io_runner() = responsive_io;
        enabled = true;
        poll();
    }
};
static void program(Program **make) { *make = new Install; }
} // namespace pidos_storage
void PiDosUsbStoragePoll() { pidos_storage::poll(); }
void PiDosUsbStorageShutdown() { pidos_storage::enabled = false; }
#endif
#endif
