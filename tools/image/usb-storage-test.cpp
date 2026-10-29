/* Production adapter fixture. Only DOS plumbing and kernel mount/I/O faults are simulated. */
#include <algorithm>
#include <atomic>
#include <cassert>
#include <cerrno>
#include <chrono>
#include <climits>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <fstream>
#include <iconv.h>
#include <map>
#include <memory>
#include <set>
#include <sstream>
#include <string>
#include <thread>
#include <vector>
#include <blkid/blkid.h>
#include <dirent.h>
#include <fcntl.h>
#include <strings.h>
#include <spawn.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <sys/sysmacros.h>
#include <unistd.h>

static std::string root;
static std::map<std::string, dev_t> devices, mounts;
static int syncs = 0, fsyncs = 0;
static bool fail_fsync = false, fail_syncfs = false, readonly_mount = false;
static bool busy_mount = false, fail_label_tool = false;
static bool exclusive_label_device = false;
static bool fail_remount = false, kernel_readonly = false;
static std::thread::id emulator_thread;
static bool check_io_thread = false, guest_interrupts = true, throw_during_wait = false;
static int idle_calls = 0, waiting_ticks = 0;
static std::atomic<bool> delay_next_sync{false}, delay_close{false}, io_waiting{false}, release_io{false};
static void delayed_io()
{
    io_waiting.store(true);
    while (!release_io.load()) std::this_thread::yield();
    io_waiting.store(false);
}
void CALLBACK_IdleNoInts()
{
    assert(std::this_thread::get_id() == emulator_thread);
    assert(!guest_interrupts); // No guest DOS reentrancy while a host FILE is borrowed.
    ++idle_calls;
    if (io_waiting.load()) {
        if (throw_during_wait) {
            throw_during_wait = false;
            release_io.store(true);
            throw 123;
        }
        if (++waiting_ticks >= 8) release_io.store(true);
    }
    std::this_thread::yield();
}
static std::chrono::steady_clock::time_point clock_now;
static auto test_now() { return clock_now; }
static int test_stat(const char *path, struct stat *st)
{
    int result = ::stat(path, st);
    if (!result && devices.count(path)) { st->st_mode = S_IFBLK | 0600; st->st_rdev = devices[path]; }
    return result;
}
static int test_fstat(int fd, struct stat *st)
{
    int result = ::fstat(fd, st);
    char link[PATH_MAX] = {};
    const std::string path = "/proc/self/fd/" + std::to_string(fd);
    const auto length = readlink(path.c_str(), link, sizeof(link) - 1);
    if (!result && length > 0 && mounts.count(link)) st->st_dev = mounts[link];
    if (!result && length > 0 && devices.count(link)) {
        st->st_mode = S_IFBLK | 0600; st->st_rdev = devices[link];
    }
    return result;
}
static int test_mount(const char *source, const char *target, const char *type, unsigned long flags, const void *)
{
    assert(flags & MS_SYNCHRONOUS);
    assert(flags & MS_NODEV); assert(flags & MS_NOSUID); assert(flags & MS_NOEXEC);
    assert(!strcmp(type, "vfat") || !strcmp(type, "exfat"));
    if (fail_remount) { errno = EIO; return -1; }
    if (readonly_mount && !(flags & MS_RDONLY)) { errno = EROFS; return -1; }
    mounts[target] = devices.at(source);
    return 0;
}
static int test_umount2(const char *target, int) { mounts.erase(target); return 0; }
static int test_umount(const char *target)
{ if (busy_mount) { errno = EBUSY; return -1; } return test_umount2(target, 0); }
static int test_open(const char *path, int flags)
{
    if (devices.count(path) && (flags & O_ACCMODE) == O_RDWR)
        exclusive_label_device = flags & O_EXCL;
    return ::open(path, flags);
}
static int test_fstatvfs(int fd, struct statvfs *status)
{
    const int result = ::fstatvfs(fd, status);
    if (!result && kernel_readonly) status->f_flag |= ST_RDONLY;
    return result;
}
static int test_spawn(pid_t *pid, const char *path, const posix_spawn_file_actions_t *actions,
                      const posix_spawnattr_t *attributes, char *const argv[], char *const env[])
{
    if (fail_label_tool) return EIO;
    // A regular-file fixture cannot enforce block-device exclusive claims.
    // Simulate exfatlabel reopening our pinned device with its own O_EXCL.
    if (strstr(path,"exfatlabel") && exclusive_label_device) return EBUSY;
    return posix_spawn(pid, path, actions, attributes, argv, env);
}
static int test_fsync(int fd)
{
    assert(!check_io_thread || std::this_thread::get_id() != emulator_thread);
    if (delay_next_sync.exchange(false)) delayed_io();
    ++fsyncs;
    if (fail_fsync) { errno = EIO; return -1; }
    return ::fsync(fd);
}
static int test_syncfs(int)
{
    assert(!check_io_thread || std::this_thread::get_id() != emulator_thread);
    ++syncs;
    if (fail_syncfs) { errno = EIO; return -1; }
    return 0;
}
static int test_fclose(FILE *stream)
{
    if (delay_close.exchange(false)) delayed_io();
    return ::fclose(stream);
}
#define stat(path, st) test_stat(path, st)
#define fstat(fd, st) test_fstat(fd, st)
#define mount test_mount
#define umount test_umount
#define umount2 test_umount2
#define fsync test_fsync
#define syncfs test_syncfs
#define fclose test_fclose
#define posix_spawn test_spawn
#define open test_open
#define fstatvfs test_fstatvfs
#define GETFLAG(flag) guest_interrupts
#define SETFLAGBIT(flag, value) (guest_interrupts = (value))
#define LINUX 1
#define DOS_DRIVES 26
#define DOSERR_DRIVE_NOT_READY 21
#define DOSERR_WRITE_PROTECTED 19
#define DOSERR_ACCESS_DENIED 5
#define DOSERR_NO_MORE_FILES 18
#define DOSERR_DATA_INVALID 13
#define DOSERR_INVALID_DRIVE 15
#define DOS_ATTR_VOLUME 8
static char *convert_label(const char *value, const char *to, const char *from) {
    static char buffer[4096]; char *input=const_cast<char *>(value), *output=buffer;
    size_t left=strlen(value), available=sizeof(buffer)-1;
    iconv_t conversion=iconv_open(to,from); assert(conversion != (iconv_t)-1);
    const bool ok=iconv(conversion,&input,&left,&output,&available) != size_t(-1);
    iconv_close(conversion); *output=0; return ok ? buffer : nullptr;
}
static char *CodePageGuestToHost(const char *value) { return convert_label(value,"UTF-8","CP437"); }
static char *CodePageHostToGuest(const char *value) { return convert_label(value,"CP437","UTF-8"); }
static char *DBCS_upcase(char *value)
{ for (char *p = value; *p; ++p) *p = std::toupper(static_cast<unsigned char>(*p)); return value; }
#define LFN_FILEFIND_MAX 255
#define LOG_MSG(...) do { fprintf(stderr, __VA_ARGS__); fputc('\n', stderr); } while (false)
using Bits = long;
static int error;
static void DOS_SetError(int value) { error = value; }
static bool dos_kernel_disabled = false;
static int lfn_filefind_handle = 258;
static const bool IS_PC98_ARCH = false;
static uint8_t IO_Read(unsigned) { return 0; }
static void IO_Write(unsigned, uint8_t) {}
static struct { struct { unsigned mediaid = 0; } tables; unsigned loaded_codepage = 437; } dos;
static unsigned Real2Phys(unsigned value) { return value; }
static void mem_writeb(unsigned, unsigned) {}
enum { OPEN_READ = 0, OPEN_WRITE = 1, OPEN_READWRITE = 2, OPEN_READ_NO_MOD = 4 };
struct FileStat_Block {};
struct DOS_DTA {
    uint32_t epoch = 0;
    void SetDirIDCluster(uint32_t value) { epoch = value; }
    uint32_t GetDirIDCluster() { return epoch; }
};
class DOS_File {
public:
    virtual ~DOS_File() = default;
    virtual bool Read(uint8_t *, uint16_t *) { return false; }
    virtual bool Write(const uint8_t *, uint16_t *) { return false; }
    virtual bool Seek(uint32_t *, uint32_t) { return false; }
    virtual bool LockFile(uint8_t, uint32_t, uint16_t) { return false; }
    virtual bool Close() { return true; }
    virtual uint32_t GetSeekPos() { return 0; }
    virtual bool UpdateDateTimeFromHost() { return true; }
    virtual void Flush() {}
    virtual bool FlushSucceeded() const { return true; }
    uint32_t flags = OPEN_READWRITE;
    uint16_t attr = 0, time = 0, date = 0;
    int refCtr = 1;
    bool newtime = false, open = true;
};
class LocalFile : public DOS_File {
    std::string filename;
public:
    const char *name;
    FILE *fhandle;
    LocalFile(const char *value, FILE *handle) : filename(value), name(filename.c_str()), fhandle(handle) {}
    void FlagReadOnlyMedium() {}
    bool UpdateLocalDateTime() { date = (46 << 9) | (10 << 5) | 29; time = 0; newtime = true; return true; }
    bool Seek(uint32_t *pos, uint32_t type) override
    {
        if (fseeko(fhandle, int32_t(*pos), type == 0 ? SEEK_SET : type == 1 ? SEEK_CUR : SEEK_END)) return false;
        *pos = uint32_t(ftello(fhandle)); return true;
    }
    uint32_t GetSeekPos() override { return uint32_t(ftello(fhandle)); }
};
struct Cache {
    std::string label;
    void SetLabel(const char *value, bool, bool) { label = value; }
};
class localDrive {
protected:
    bool native_dos_attributes = false;
    std::string base;
    Cache dirCache;
public:
    bool readonly = false;
    localDrive(const char *path, uint16_t, uint8_t, uint16_t, uint16_t, uint8_t, std::vector<std::string> &) : base(path) {}
    virtual ~localDrive() = default;
    virtual bool FileOpen(DOS_File **file, const char *name, uint32_t flags)
    {
        FILE *stream = fopen((base + name).c_str(), (flags & 15) == OPEN_READ ? "rb" : "rb+");
        if (!stream) return false;
        *file = new LocalFile(name, stream); (*file)->flags = flags; return true;
    }
    virtual bool FileCreate(DOS_File **file, const char *name, uint16_t)
    {
        FILE *stream = fopen((base + name).c_str(), "wb+");
        if (!stream) return false;
        *file = new LocalFile(name, stream); return true;
    }
    virtual bool FileUnlink(const char *name) { return !unlink((base + name).c_str()); }
    virtual bool RemoveDir(const char *name) { return !rmdir((base + name).c_str()); }
    virtual bool MakeDir(const char *name) { return !mkdir((base + name).c_str(), 0700); }
    virtual bool Rename(const char *a, const char *b) { return !rename((base + a).c_str(), (base + b).c_str()); }
    virtual bool SetFileAttr(const char *, uint16_t) { return true; }
    virtual bool TestDir(const char *) { return true; }
    virtual bool FileExists(const char *) { return true; }
    virtual bool FileStat(const char *, FileStat_Block *) { return true; }
    virtual bool GetFileAttr(const char *, uint16_t *) { return true; }
    virtual bool GetFileAttrEx(char *, struct stat *) { return true; }
    virtual unsigned long GetCompressedSize(char *) { return 0; }
    virtual bool AllocationInfo(uint16_t *, uint8_t *, uint16_t *, uint16_t *) { return true; }
    virtual bool AllocationInfo64(uint32_t *, uint32_t *, uint64_t *, uint64_t *) { return true; }
    virtual bool FindFirst(const char *, DOS_DTA &dta, bool) { return FindNext(dta); }
    virtual bool FindNext(DOS_DTA &) { return true; }
    virtual FILE *GetSystemFilePtr(const char *, const char *) { return nullptr; }
    virtual bool GetSystemFilename(char *, const char *) { return false; }
    virtual const char *GetLabel() { return dirCache.label.c_str(); }
    virtual void SetLabel(const char *label, bool cdrom, bool updatable) { dirCache.SetLabel(label, cdrom, updatable); }
    virtual bool SetLabelChecked(const char *) { return true; }
    virtual void EmptyCache() {}
    virtual bool isRemovable() { return false; }
    virtual Bits UnMount() { return 0; }
};
static localDrive *Drives[DOS_DRIVES] = {};
class Program { public: virtual ~Program() = default; virtual void Run() = 0; };
#include "pidos_usb_storage_dos.h"
#include "pidos_system_drive_dos.h"
#include "checked-label.h"

static void advance() { clock_now += std::chrono::seconds(6); PiDosUsbStoragePoll(); }
static void put(const std::string &path, const std::string &value) { std::ofstream(path) << value; }
static std::string sys(const std::string &name)
{ return pidos_storage::canonical(root + "/sys/class/block/" + name); }
int main(int argc, char **argv)
{
    assert(argc == 2); root = argv[1];
    emulator_thread = std::this_thread::get_id();
    pidos_storage::io_runner() = pidos_storage::responsive_io;
    check_io_thread = true;
    Program *system = nullptr;
    pidos_system::program(&system);
    unsetenv("PI_DOS_SUPERVISED"); setenv("PI_DOS_SYSTEM_STORAGE", "rw", 1);
    system->Run(); assert(!Drives[2] && !PiDosProtectedDrive(2));
    setenv("PI_DOS_SUPERVISED", "1", 1); setenv("PI_DOS_SYSTEM_STORAGE", "none", 1);
    system->Run(); assert(!Drives[2]); // Recovery never invents C:.
    assert(PiDosProtectedDrive(2) && PiDosProtectedDrive(25) && !PiDosProtectedDrive(4));
    assert(strstr(PiDosUnavailableDriveMessage(), "unavailable"));
    assert(strstr(PiDosUnavailableDriveMessage(), "list available drives"));
    setenv("PI_DOS_SYSTEM_STORAGE", "ro", 1);
    system->Run(); assert(Drives[2] && Drives[2]->readonly);
    assert(!strcmp(Drives[2]->GetLabel(), "SYSTEM") && Drives[2]->UnMount() != 0);
    auto original_c = Drives[2]; setenv("PI_DOS_SYSTEM_STORAGE", "rw", 1);
    system->Run(); assert(Drives[2] == original_c && Drives[2]->readonly);
    delete Drives[2]; Drives[2] = nullptr;
    system->Run(); assert(Drives[2] && !Drives[2]->readonly);
    DOS_File *root_file = nullptr;
    assert(Drives[2]->FileCreate(&root_file, "ROOT.DAT", 0));
    fclose(static_cast<LocalFile *>(root_file)->fhandle); delete root_file;
    assert(pidos_storage::exists(root + "/system-root/ROOT.DAT"));
    assert(!pidos_storage::exists(root + "/system-root/c"));
    delete Drives[2]; Drives[2] = nullptr; delete system;
    std::ifstream numbers(root + "/numbers");
    std::string name; unsigned number;
    while (numbers >> name >> number) devices[root + "/dev/" + name] = makedev(8, number);
    setenv("PI_DOS_DATA_DEVICE", (root + "/dev/sdh").c_str(), 1);
    // Reserve a manually occupied letter and the internal Z: drive.
    std::vector<std::string> options;
    localDrive manual("", 512, 1, 1, 1, 0xf8, options);
    Drives[5] = &manual; Drives[25] = &manual;
    pidos_storage::enabled = true;
    advance();
    // Three MBR FAT variants, superfloppy exFAT, logical FAT16 and GPT exFAT.
    assert(pidos_storage::managed[4]);
    assert(!pidos_storage::managed[5] && Drives[5] == &manual);
    for (int i = 6; i <= 10; ++i) assert(pidos_storage::managed[i]);
    assert(!Drives[11]);
    assert(mounts.size() == 6);
    assert(pidos_storage::managed[4]->key().find("sda1") != std::string::npos);
    assert(pidos_storage::managed[7]->key().find("sda3") != std::string::npos);
    assert(pidos_storage::managed[8]->key().find("sdb") != std::string::npos);
    assert(pidos_storage::managed[9]->key().find("sdc5") != std::string::npos);
    assert(pidos_storage::managed[10]->key().find("sdg2") != std::string::npos);
    assert(!DOS_SetVolumeLabel(25, "SYSTEM") && error == DOSERR_WRITE_PROTECTED);
    assert(!DOS_SetVolumeLabel(24, "MISSING") && error == DOSERR_INVALID_DRIVE);
    for (const char *bad : {"TOO MANY CHARS", "BAD.NAME", "BAD?NAME", " LEADING", "BAD\x01"})
        assert(!DOS_SetVolumeLabel(4, bad) && error == DOSERR_DATA_INVALID);
    assert(DOS_SetVolumeLabel(4, "mixed case ") && !strcmp(Drives[4]->GetLabel(), "MIXED CASE"));

    // Actual fatlabel/exfatlabel tools update only the sparse filesystem images.
    for (const int drive : {4, 6, 7, 8, 9, 10}) {
        assert(Drives[drive]->SetLabelChecked("USB DATA"));
        assert(!strcmp(Drives[drive]->GetLabel(), "USB DATA"));
        assert(Drives[drive]->SetLabelChecked(""));
        assert(!*Drives[drive]->GetLabel());
        assert(DOS_SetVolumeLabel(drive, "CAF\x90")); // CP437 capital E acute.
        assert(!strcmp(Drives[drive]->GetLabel(), "CAF\x90"));
        assert(Drives[drive]->FileCreate(nullptr, "FORMATTED", DOS_ATTR_VOLUME));
        assert(!strcmp(Drives[drive]->GetLabel(), "FORMATTED"));
    }
    busy_mount = true;
    assert(!Drives[4]->SetLabelChecked("BUSY") && error == 32);
    busy_mount = false;
    assert(!strcmp(Drives[4]->GetLabel(), "FORMATTED"));
    fail_label_tool = true;
    assert(!Drives[4]->SetLabelChecked("FAILED")); fail_label_tool = false;
    assert(!strcmp(Drives[4]->GetLabel(), "FORMATTED") && mounts.size() == 6);
    assert(Drives[4]->SetLabelChecked("RECOVERED"));
    fail_remount = true;
    assert(!Drives[6]->SetLabelChecked("LOST MOUNT") && error == DOSERR_DRIVE_NOT_READY);
    fail_remount = false; advance();
    assert(Drives[6] && !strcmp(Drives[6]->GetLabel(), "LOST MOUNT"));
    kernel_readonly = true;
    assert(!Drives[7]->SetLabelChecked("NO WRITE") && error == DOSERR_WRITE_PROTECTED);
    kernel_readonly = false;
    assert(!strcmp(Drives[7]->GetLabel(), "FORMATTED"));
    DOS_File *label_busy = nullptr;
    assert(Drives[4]->FileCreate(&label_busy, "BUSY.DAT", 0));
    assert(!Drives[4]->SetLabelChecked("OPEN FILE") && error == 32);
    assert(label_busy->Close()); delete label_busy;

    DOS_File *completed = nullptr;
    assert(Drives[4]->FileCreate(&completed, "COMPLETE.DAT", 0));
    uint8_t contents[] = {5, 6, 7, 8}; uint16_t written = sizeof(contents);
    assert(completed->Write(contents, &written));
    assert(completed->Close()); delete completed;
    assert(Drives[4]->FileOpen(&completed, "COMPLETE.DAT", OPEN_READ));
    uint8_t copied[4] = {}; written = sizeof(copied);
    assert(completed->Read(copied, &written) && written == sizeof(copied));
    assert(!memcmp(contents, copied, sizeof(copied)));
    assert(completed->Close()); delete completed;

    DOS_File *old = nullptr;
    assert(Drives[4]->FileCreate(&old, "COPIED.DAT", 0));
    uint8_t data[] = {1, 2, 3, 4}; uint16_t count = sizeof(data);
    const int previous_fsyncs = fsyncs;
    const int previous_idle_calls = idle_calls;
    delay_next_sync.store(true); release_io.store(false); waiting_ticks = 0;
    assert(old->Write(data, &count) && count == sizeof(data) && fsyncs > previous_fsyncs);
    assert(idle_calls > previous_idle_calls && waiting_ticks >= 8 && !io_waiting.load());
    assert(guest_interrupts); // Completion still waits for fsync, with IF restored.
    guest_interrupts = false;
    assert(pidos_storage::host_io([] { return 7; }) == 7 && !guest_interrupts);
    guest_interrupts = true;

    // A quit/reset during a slow write waits for its borrowed buffer before unwinding.
    delay_next_sync.store(true); release_io.store(false); throw_during_wait = true;
    bool interrupted = false;
    try { count = sizeof(data); old->Write(data, &count); }
    catch (int value) { assert(value == 123); interrupted = true; }
    assert(interrupted && !io_waiting.load() && guest_interrupts);
    uint32_t offset = 2;
    assert(old->Seek(&offset, 0)); count = 0;
    assert(old->Write(data, &count)); // Zero-length DOS write truncates durably.
    struct stat st; assert(!::fstat(static_cast<LocalFile *>(old)->fhandle ? fileno(static_cast<LocalFile *>(old)->fhandle) : -1, &st));
    assert(st.st_size == 2);
    DOS_DTA search;
    assert(Drives[4]->FindFirst("", search, false));
    DOS_DTA shared;
    lfn_filefind_handle = 1; assert(Drives[4]->FindFirst("", shared, false));
    lfn_filefind_handle = 2; assert(Drives[9]->FindFirst("", shared, false));
    lfn_filefind_handle = 1; assert(Drives[4]->FindNext(shared));
    lfn_filefind_handle = 258;
    auto preserved = Drives[8];
    // Reuse a kernel device name before the next scan: old handles already fail.
    put(sys("sda") + "/diskseq", "2");
    count = 4; assert(!old->Read(data, &count) && count == 0 && error == DOSERR_DRIVE_NOT_READY);
    advance();
    assert(Drives[6] != nullptr); // Same disk's other partitions receive fresh drive objects.
    assert(Drives[8] == preserved); // Other connected disks retain their letters.
    assert(Drives[4] && !Drives[4]->FindNext(search) && error == DOSERR_NO_MORE_FILES);
    lfn_filefind_handle = 1;
    assert(!Drives[4]->FindNext(shared) && error == DOSERR_NO_MORE_FILES);
    lfn_filefind_handle = 258;
    count = 4; assert(!old->Write(data, &count) && count == 0);
    assert(!old->Close()); delete old;

    // A disconnected volume frees its letter without moving remaining volumes.
    auto stable = Drives[9];
    put(sys("sdb") + "/diskseq", ""); advance();
    assert(!Drives[8] && Drives[9] == stable);
    put(sys("sdb") + "/diskseq", "2"); readonly_mount = true; advance(); readonly_mount = false;
    assert(Drives[8] && Drives[9] == stable);
    assert(!strcmp(Drives[8]->GetLabel(), "FORMATTED")); // Read back after reconnect.
    DOS_File *readonly_file = nullptr;
    assert(!Drives[8]->FileCreate(&readonly_file, "NO.DAT", 0) && error == DOSERR_WRITE_PROTECTED);
    assert(!Drives[8]->SetLabelChecked("READ ONLY") && error == DOSERR_WRITE_PROTECTED);

    DOS_File *failed = nullptr;
    assert(Drives[4]->FileCreate(&failed, "FAILED.DAT", 0));
    fail_fsync = true; count = 4;
    assert(!failed->Write(data, &count));
    failed->Flush(); assert(!failed->FlushSucceeded());
    fail_fsync = false; assert(!failed->Close()); delete failed;
    assert(Drives[4]->FileCreate(&failed, "CLOSE.DAT", 0));
    fail_syncfs = true; assert(!failed->Close()); fail_syncfs = false; delete failed;
    fail_syncfs = true;
    assert(!Drives[4]->MakeDir("FAILED-DIR"));
    fail_syncfs = false;

    assert(Drives[4]->FileCreate(&failed, "CLOSE-QUIT.DAT", 0));
    delay_close.store(true); release_io.store(false); throw_during_wait = true;
    interrupted = false;
    try { failed->Close(); }
    catch (int value) { assert(value == 123); interrupted = true; }
    assert(interrupted && !io_waiting.load() && guest_interrupts);
    assert(!static_cast<LocalFile *>(failed)->fhandle);
    delete failed; // fclose completed once; destruction must not close it again.

    // Optional eject refuses an open handle, then stays ejected until reconnect.
    assert(Drives[4]->FileCreate(&failed, "EJECT.DAT", 0));
    assert(Drives[4]->UnMount() != 0);
    assert(failed->Close()); delete failed;
    assert(Drives[4]->UnMount() == 0); Drives[4] = nullptr;
    advance(); assert(!Drives[4]);
    put(sys("sda") + "/diskseq", "3"); advance(); assert(Drives[4]);
    assert(syncs > 0);
    for (int i = 4; i < DOS_DRIVES; ++i) {
        if (Drives[i] && Drives[i] != &manual) delete Drives[i];
        Drives[i] = nullptr;
    }
    assert(mounts.empty());
    for (int i = 4; i < DOS_DRIVES; ++i) Drives[i] = &manual;
    clock_now += std::chrono::seconds(31); advance();
    assert(mounts.empty()); // Letter exhaustion never steals an existing drive.
    for (int i = 4; i < DOS_DRIVES; ++i) Drives[i] = nullptr;
    Program *program = nullptr; pidos_storage::program(&program); delete program;
    return 0;
}
