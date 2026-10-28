/* Linux USB floppy transport. Original Pi-DOS code; license pending. */
#ifndef PI_DOS_USB_FLOPPY_LINUX_H
#define PI_DOS_USB_FLOPPY_LINUX_H

#include <algorithm>
#include <array>
#include <cerrno>
#include <cctype>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <fcntl.h>
#include <glob.h>
#include <linux/fs.h>
#include <memory>
#include <map>
#include <scsi/sg.h>
#include <string>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/sysmacros.h>
#include <unistd.h>

/* The cross-toolchain's libc headers predate this Linux 5.15 ioctl; the
 * appliance kernel is 6.12. Keep the exact Linux UAPI encoding. */
#ifndef BLKGETDISKSEQ
#define BLKGETDISKSEQ _IOR(0x12, 128, __u64)
#endif

namespace pidos_floppy {

inline std::string line(const std::string &path)
{
    char value[256] = {};
    FILE *file = fopen(path.c_str(), "r");
    if (file) { if (!fgets(value, sizeof(value), file)) value[0] = 0; fclose(file); }
    value[strcspn(value, "\r\n")] = 0;
    return value;
}

inline std::string usb_interface(const std::string &path)
{
    char resolved[4096];
    if (!realpath((path + "/device").c_str(), resolved)) return {};
    std::string parent = resolved;
    while (!parent.empty()) {
        if (line(parent + "/bInterfaceClass") == "08") return parent;
        const auto slash = parent.find_last_of('/');
        if (slash == std::string::npos) break;
        parent.resize(slash);
    }
    return {};
}

/* Only whole, removable SCSI disks on a USB mass-storage interface qualify.
 * UFI identifies normal USB floppy drives. Some older drives use SFF-8070i
 * or transparent SCSI instead; accept those only with an explicit floppy model.
 * Capacity and FAT12 geometry are checked again before any sector can be written. */
inline bool candidate(const std::string &path)
{
    if (line(path + "/removable") != "1" || line(path + "/device/type") != "0" ||
        !line(path + "/partition").empty()) return false;
    std::string model = line(path + "/device/model");
    std::transform(model.begin(), model.end(), model.begin(),
                   [](unsigned char c) { return std::tolower(c); });
    const bool named = model.find("floppy") != std::string::npos ||
                       model.find("usb-fdu") != std::string::npos ||
                       model.find("ufd") != std::string::npos;
    const std::string interface = usb_interface(path);
    return !interface.empty() && (line(interface + "/bInterfaceSubClass") == "04" || named);
}

inline bool mounted(dev_t device)
{
    FILE *file = fopen("/proc/self/mountinfo", "r");
    if (!file) return true; // Do not allow raw writes if exclusivity is unknown.
    char row[8192];
    bool found = false;
    while (fgets(row, sizeof(row), file)) {
        unsigned int maj, min;
        if (sscanf(row, "%*u %*u %u:%u", &maj, &min) == 2 &&
            maj == major(device) && min == minor(device)) { found = true; break; }
    }
    fclose(file);
    return found;
}

enum class Result { ok, changed, absent, protected_disk, unsupported, error };

struct IoTiming {
    std::chrono::steady_clock::time_point started = {}, finished = {};
};
using IoExecutor = int (*)(int, sg_io_hdr_t *, int &, IoTiming *);
static IoExecutor io_executor = nullptr;
inline int direct_io(int fd, sg_io_hdr_t *io, int &error, IoTiming *timing = nullptr)
{
    if (timing) timing->started = std::chrono::steady_clock::now();
    const int result = ioctl(fd, SG_IO, io);
    error = result < 0 ? errno : 0;
    if (timing) timing->finished = std::chrono::steady_clock::now();
    return result;
}

class Device {
    int fd = -1;
    bool writable_fd = true;
    uint64_t sequence = 0;
    std::chrono::steady_clock::time_point checked = {};
    struct Track {
        std::array<unsigned char, 64 * 512> data = {};
        uint32_t first = UINT32_MAX, count = 0;
        uint64_t age = 0;
    };
    std::array<Track, 4> tracks;
    uint64_t cache_age = 0;
    uint32_t last_data_sector = UINT32_MAX;
    unsigned int max_transfer = 64; // At most 32 KiB, also bounded by the kernel queue.
    bool track_only = false;
    std::map<uint32_t, std::array<unsigned char, 512>> pending;
    /* Confirmed on-disk metadata, separate from read windows which also
     * contain staged writes. Never use a dirty read window to elide a write. */
    std::map<uint32_t, std::array<unsigned char, 512>> committed_metadata;
    bool batching = false;
    uint32_t root_start = 0, data_start = 0;
    bool trace = false;
    uint64_t command_id = 0;
    std::chrono::steady_clock::time_point command_end = {};
public:
    const std::string path;
    bool valid = true;
    bool readonly = true;
    uint32_t blocks = 0;
    uint32_t heads = 0, sectors = 0, cylinders = 0;

    explicit Device(const std::string &name) : path(name)
    {
        const char *diagnostics = getenv("PI_DOS_DIAGNOSTICS");
        trace = diagnostics && !strcmp(diagnostics, "1");
        /* Exclusive access also refuses an already mounted block device.
         * SG_IO bypasses the Linux block cache: a replacement disk must never
         * receive dirty blocks or cached reads belonging to the previous disk. */
        fd = open(path.c_str(), O_RDWR | O_NONBLOCK | O_EXCL | O_CLOEXEC);
        if (fd < 0) {
            writable_fd = false;
            fd = open(path.c_str(), O_RDONLY | O_NONBLOCK | O_EXCL | O_CLOEXEC);
        }
        struct stat status = {};
        if (fd < 0 || fstat(fd, &status) || !S_ISBLK(status.st_mode) || mounted(status.st_rdev)) {
            if (fd >= 0) close(fd);
            fd = -1;
            valid = false;
        }
        if (valid && ioctl(fd, BLKGETDISKSEQ, &sequence)) invalidate();
    }
    ~Device() { if (fd >= 0) close(fd); }
    Device(const Device &) = delete;
    Device &operator=(const Device &) = delete;

    void configure_io(const std::string &sys_block)
    {
        const std::string value = line(sys_block + "/queue/max_sectors_kb");
        char *end = nullptr;
        const unsigned long kb = strtoul(value.c_str(), &end, 10);
        if (end != value.c_str() && !*end && kb)
            max_transfer = kb >= 32 ? 64 : static_cast<unsigned int>(kb * 2);
    }

    void trace_usb(const std::string &sys_block)
    {
        if (!trace) return;
        const std::string interface = usb_interface(sys_block);
        if (interface.empty()) return;
        const std::string usb = interface.substr(0, interface.find_last_of('/'));
        auto value = [](const std::string &file) {
            const std::string result = line(file);
            return result.empty() ? std::string("unknown") : result;
        };
        char resolved[4096];
        const std::string driver = realpath((interface + "/driver").c_str(), resolved) ?
                                   std::string(resolved) : "unknown";
        fprintf(stderr, "Pi-DOS USB floppy device: %s trace_version=2 usb=%s vendor=%s product=%s revision=%s "
                "speed_mbps=%s subclass=%s protocol=%s driver=%s model=\"%s\" power_control=%s autosuspend_ms=%s max_sectors=%u\n",
                path.c_str(), usb.c_str(), value(usb + "/idVendor").c_str(), value(usb + "/idProduct").c_str(),
                value(usb + "/bcdDevice").c_str(), value(usb + "/speed").c_str(),
                value(interface + "/bInterfaceSubClass").c_str(), value(interface + "/bInterfaceProtocol").c_str(),
                driver.c_str(), value(sys_block + "/device/model").c_str(),
                value(usb + "/power/control").c_str(), value(usb + "/power/autosuspend_delay_ms").c_str(), max_transfer);
        glob_t endpoints = {};
        if (!glob((interface + "/ep_*").c_str(), 0, nullptr, &endpoints)) {
            for (size_t i = 0; i < endpoints.gl_pathc; ++i) {
                const std::string ep = endpoints.gl_pathv[i];
                /* bInterval is hexadecimal in sysfs. interval is the kernel's
                 * decoded descriptor value, not a measured host poll period. */
                fprintf(stderr, "Pi-DOS USB floppy endpoint: %s address=%s type=%s direction=%s "
                        "attributes_hex=%s max_packet_hex=%s bInterval_hex=%s descriptor_interval=%s\n",
                        path.c_str(), value(ep + "/bEndpointAddress").c_str(), value(ep + "/type").c_str(),
                        value(ep + "/direction").c_str(), value(ep + "/bmAttributes").c_str(),
                        value(ep + "/wMaxPacketSize").c_str(), value(ep + "/bInterval").c_str(),
                        value(ep + "/interval").c_str());
            }
        }
        globfree(&endpoints);
    }

    void invalidate()
    {
        valid = false;
        clear_cache(); pending.clear(); batching = false;
        if (fd >= 0) close(fd);
        fd = -1;
    }

    void clear_cache()
    {
        for (auto &track : tracks) track.first = UINT32_MAX;
        last_data_sector = UINT32_MAX;
        committed_metadata.clear();
    }
    void remember_metadata(uint32_t first, unsigned int count, const void *data)
    {
        for (unsigned int i = 0; i < count && first + i < data_start; ++i) {
            const uint32_t number = first + i;
            if (!number) continue; // Boot/raw writes must always reach the device.
            auto at = committed_metadata.find(number);
            // Bound comparison storage to 32 KiB, even for unusual valid BPBs.
            if (at == committed_metadata.end()) {
                if (committed_metadata.size() >= 64) continue;
                at = committed_metadata.emplace(number, std::array<unsigned char, 512>{}).first;
            }
            memcpy(at->second.data(), static_cast<const unsigned char *>(data) + i * 512, 512);
        }
    }
    Track *cached(uint32_t sector)
    {
        for (auto &track : tracks) {
            if (track.first != UINT32_MAX && sector >= track.first && sector - track.first < track.count) {
                track.age = ++cache_age; return &track;
            }
        }
        return nullptr;
    }

    Result command(unsigned char *cdb, unsigned int length, void *data = nullptr,
                   unsigned int size = 0, bool write = false)
    {
        if (!valid) return Result::absent;
        const auto check_start = trace ? std::chrono::steady_clock::now() : std::chrono::steady_clock::time_point{};
        uint64_t current = 0;
        if (ioctl(fd, BLKGETDISKSEQ, &current) || current != sequence) {
            invalidate();
            return Result::changed;
        }
        const auto check_end = trace ? std::chrono::steady_clock::now() : std::chrono::steady_clock::time_point{};
        unsigned char sense[32] = {};
        sg_io_hdr_t io = {};
        io.interface_id = 'S';
        io.cmdp = cdb;
        io.cmd_len = static_cast<unsigned char>(length);
        io.dxfer_direction = !size ? SG_DXFER_NONE : write ? SG_DXFER_TO_DEV : SG_DXFER_FROM_DEV;
        io.dxferp = data;
        io.dxfer_len = size;
        io.sbp = sense;
        io.mx_sb_len = sizeof(sense);
        io.timeout = 3000;
        const auto start = trace ? std::chrono::steady_clock::now() : std::chrono::steady_clock::time_point{};
        IoTiming timing;
        int saved_errno = 0;
        const int result = io_executor ? io_executor(fd, &io, saved_errno, trace ? &timing : nullptr) :
                                        direct_io(fd, &io, saved_errno, trace ? &timing : nullptr);
        if (trace) {
            const auto end = std::chrono::steady_clock::now();
            const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
            const auto gap = command_end.time_since_epoch().count() ?
                std::chrono::duration_cast<std::chrono::milliseconds>(start - command_end).count() : 0;
            command_end = end;
            const uint32_t lba = cdb[0] == 0x28 || cdb[0] == 0x2a ?
                (uint32_t(cdb[2]) << 24) | (uint32_t(cdb[3]) << 16) | (uint32_t(cdb[4]) << 8) | cdb[5] : 0;
            auto us = [](std::chrono::steady_clock::duration duration) {
                return static_cast<long long>(std::chrono::duration_cast<std::chrono::microseconds>(duration).count());
            };
            const bool transfer = cdb[0] == 0x28 || cdb[0] == 0x2a;
            const char *region = !transfer ? "control" : !data_start ? "raw" : !lba ? "boot" :
                                 lba < root_start ? "fat" : lba < data_start ? "root" : "data";
            fprintf(stderr, "Pi-DOS USB floppy: %s op=%02x lba=%u bytes=%u gap_ms=%lld elapsed_ms=%lld "
                    "status=%02x host=%04x driver=%04x resid=%d sense=%02x/%02x/%02x errno=%d "
                    "id=%llu t_ms=%lld region=%s check_us=%lld queue_us=%lld ioctl_us=%lld resume_us=%lld sg_ms=%u\n",
                    path.c_str(), cdb[0], lba, size, static_cast<long long>(gap), static_cast<long long>(elapsed),
                    io.status, io.host_status, io.driver_status, io.resid, sense[2], sense[12], sense[13], saved_errno,
                    static_cast<unsigned long long>(++command_id),
                    static_cast<long long>(std::chrono::duration_cast<std::chrono::milliseconds>(start.time_since_epoch()).count()),
                    region, us(check_end - check_start), us(timing.started - start),
                    us(timing.finished - timing.started), us(end - timing.finished), io.duration);
        }
        if (result < 0 || io.host_status) {
            if (result < 0 && !io.host_status && (saved_errno == EINVAL || saved_errno == EMSGSIZE) &&
                (cdb[0] == 0x28 || cdb[0] == 0x2a)) return Result::unsupported;
            invalidate();
            return Result::absent;
        }
        if ((io.info & SG_INFO_OK_MASK) == SG_INFO_OK && !io.status && !io.driver_status) {
            if (io.resid) return Result::error;
            /* A completed real transfer already establishes media readiness.
             * Start the probe interval after completion, not before a slow read. */
            if (cdb[0] == 0x28 || cdb[0] == 0x2a) checked = std::chrono::steady_clock::now();
            return Result::ok;
        }
        unsigned int key = 0, asc = 0;
        if (io.sb_len_wr >= 14 && (sense[0] & 0x7e) == 0x70) {
            key = sense[2] & 15; asc = sense[12];
        } else if (io.sb_len_wr >= 4 && (sense[0] & 0x7e) == 0x72) {
            key = sense[1] & 15; asc = sense[2];
        }
        if (key == 6 || (key == 2 && asc == 0x3a)) {
            invalidate();
            return key == 6 ? Result::changed : Result::absent;
        }
        if (key == 7) { readonly = true; return Result::protected_disk; }
        if (key == 5) return Result::unsupported;
        return Result::error;
    }

    Result ready(bool force = true)
    {
        if (!valid) return Result::absent;
        const auto now = std::chrono::steady_clock::now();
        if (!force && now - checked < std::chrono::milliseconds(100)) return Result::ok;
        unsigned char cdb[12] = {}; // TEST UNIT READY (UFI uses padded 12-byte CDBs).
        const Result result = command(cdb, sizeof(cdb));
        if (result == Result::ok) checked = std::chrono::steady_clock::now();
        return result;
    }

    Result transfer_command(uint32_t number, unsigned int count, void *data, bool write)
    {
        if (!valid) return Result::absent;
        if (number >= blocks || !count || count > blocks - number) { invalidate(); return Result::error; }
        if (write && readonly) return Result::protected_disk;
        unsigned char cdb[12] = {};
        cdb[0] = write ? 0x2a : 0x28; // WRITE(10) / READ(10).
        cdb[2] = static_cast<unsigned char>(number >> 24);
        cdb[3] = static_cast<unsigned char>(number >> 16);
        cdb[4] = static_cast<unsigned char>(number >> 8);
        cdb[5] = static_cast<unsigned char>(number);
        cdb[7] = static_cast<unsigned char>(count >> 8);
        cdb[8] = static_cast<unsigned char>(count);
        return command(cdb, sizeof(cdb), data, count * 512, write);
    }

    Result sectors_io(uint32_t number, unsigned int count, void *data, bool write)
    {
        if (!valid) return Result::absent;
        if (number >= blocks || !count || count > blocks - number) { invalidate(); return Result::error; }
        unsigned int done = 0;
        while (done < count) {
            unsigned int chunk = std::min<unsigned int>(count - done, max_transfer);
            if (track_only) chunk = std::min<unsigned int>(chunk, sectors ? sectors - (number + done) % sectors : 1);
            const Result result = transfer_command(number + done, chunk,
                static_cast<unsigned char *>(data) + done * 512, write);
            if (result == Result::unsupported && chunk > 1) {
                /* Retry only a rejected request on this same medium. Never
                 * retry media changes, write protection or transport errors.
                 * First fall back to physical tracks, then smaller commands. */
                if (!track_only && sectors && chunk > sectors - (number + done) % sectors)
                    track_only = true;
                else { track_only = true; max_transfer = std::max(1u, chunk / 2); }
                if (trace) fprintf(stderr, "Pi-DOS USB floppy limits: %s max_sectors=%u track_only=1 reason=request_rejected\n",
                                   path.c_str(), max_transfer);
                continue;
            }
            if (result != Result::ok) return result;
            done += chunk;
        }
        return Result::ok;
    }

    Result sector(uint32_t number, void *data, bool write = false)
    {
        if (!valid) return Result::absent;
        if (number >= blocks) { invalidate(); return Result::error; }
        if (write) {
            if (readonly) return Result::protected_disk;
            Result result = Result::ok;
            if (batching) {
                if (pending.size() >= 256 && !pending.count(number)) {
                    result = flush_writes();
                    if (result != Result::ok) return result;
                }
                memcpy(pending[number].data(), data, 512);
            } else {
                result = sectors_io(number, 1, data, true);
                if (result == Result::ok) remember_metadata(number, 1, data);
            }
            if (result == Result::ok) {
                /* Read-ahead windows can overlap after a backwards seek. */
                for (auto &track : tracks)
                    if (track.first != UINT32_MAX && number >= track.first && number - track.first < track.count)
                        memcpy(track.data.data() + (number - track.first) * 512, data, 512);
            } else invalidate();
            return result;
        }
        auto staged = pending.find(number);
        if (staged != pending.end()) { memcpy(data, staged->second.data(), 512); return Result::ok; }
        /* Keep short/random and metadata reads small. Once data is sequential,
         * read up to 32 KiB across tracks so host round trips do not repeatedly
         * miss the next track's first sector and wait an extra revolution. */
        const Result present = ready(false);
        if (present != Result::ok) { invalidate(); return present; }
        const bool file_data = data_start && number >= data_start;
        const bool sequential = file_data && last_data_sector != UINT32_MAX && number == last_data_sector + 1;
        if (file_data) last_data_sector = number;
        const uint32_t track = file_data ? number : sectors ? number / sectors * sectors : number;
        const unsigned int span = sequential && !track_only ? max_transfer : sectors ? sectors - track % sectors : 1;
        const unsigned int count = std::min<unsigned int>(span, blocks - track);
        Track *hit = cached(number);
        if (hit) {
            uint64_t current = 0;
            if (ioctl(fd, BLKGETDISKSEQ, &current) || current != sequence) {
                invalidate(); return Result::changed;
            }
        } else {
            hit = &*std::min_element(tracks.begin(), tracks.end(), [](const Track &a, const Track &b) {
                return a.first == UINT32_MAX ? b.first != UINT32_MAX : b.first != UINT32_MAX && a.age < b.age;
            });
            hit->first = UINT32_MAX;
            const Result result = sectors_io(track, count, hit->data.data(), false);
            if (result != Result::ok) {
                /* A bad neighbour must not make an otherwise readable sector
                 * inaccessible. Retry just that sector, never a changed disk. */
                const Result single = result == Result::error ? sectors_io(number, 1, data, false) : result;
                if (single != Result::ok) invalidate();
                return single;
            }
            hit->first = track; hit->count = count; hit->age = ++cache_age;
            // Record physical contents before overlaying any pending writes.
            remember_metadata(track, count, hit->data.data());
            /* A newly fetched track may contain sectors staged before this
             * cache entry existed. Preserve them after the batch commits. */
            for (auto at = pending.lower_bound(track); at != pending.end() && at->first - track < count; ++at)
                memcpy(hit->data.data() + (at->first - track) * 512, at->second.data(), 512);
        }
        memcpy(data, hit->data.data() + (number - hit->first) * 512, 512);
        return Result::ok;
    }

    /* BIOS and track requests already specify the complete transfer. The real
     * READ/WRITE command checks media state itself; a separate TUR per sector
     * would add USB/motor latency and emulated floppy timing is unnecessary. */
    Result raw_io(uint32_t first, unsigned int count, void *data, bool write)
    {
        clear_cache();
        return sectors_io(first, count, data, write);
    }

    Result fill_track(uint32_t first, unsigned int count)
    {
        if (!count || count > 36) return Result::error;
        unsigned char data[36 * 512];
        memset(data, 0xf6, count * 512);
        clear_cache();
        const Result result = sectors_io(first, count, data, true);
        if (result != Result::ok) invalidate();
        return result;
    }

    bool begin_write()
    {
        if (!valid || readonly || batching || !sectors || sectors > 36) return false;
        batching = true; return true;
    }
    Result flush_writes()
    {
        if (!valid) return Result::absent;
        unsigned int unchanged = 0;
        const auto region = [this](uint32_t n) { return n >= data_start ? 2 : n >= root_start ? 1 : 0; };
        const auto identical = [this](uint32_t number, const std::array<unsigned char, 512> &data) {
            const auto saved = committed_metadata.find(number);
            return saved != committed_metadata.end() && data == saved->second;
        };
        for (auto at = pending.begin(); at != pending.end();) {
            /* Trim unchanged edges of each bounded transfer. Keep unchanged
             * sectors between changed ones: removing those would split one
             * USB command into two and can cost another disk revolution. */
            const uint32_t start = at->first;
            auto end = at;
            unsigned int count = 0;
            while (end != pending.end() && end->first == start + count && count < max_transfer &&
                   region(end->first) == region(start)) { ++end; ++count; }
            while (at != end && identical(at->first, at->second)) {
                at = pending.erase(at); ++unchanged;
            }
            auto tail = end;
            while (tail != at) {
                auto previous = tail; --previous;
                if (!identical(previous->first, previous->second)) break;
                tail = pending.erase(previous); ++unchanged;
            }
            at = end;
        }
        if (unchanged && pending.empty()) {
            /* A normal WRITE detects unit attention/removal itself. If every
             * write was elided, retain that check, including BLKGETDISKSEQ,
             * instead of completing solely from cached comparisons. */
            const Result result = ready(true);
            if (result != Result::ok) { invalidate(); return result; }
        }
        if (trace && unchanged)
            fprintf(stderr, "Pi-DOS USB floppy commit: %s unchanged_metadata=%u\n", path.c_str(), unchanged);
        while (!pending.empty()) {
            /* Keep file data ahead of FAT/root-directory publication, and
             * combine repeated FAT updates and contiguous sectors across tracks. */
            auto first = data_start ? pending.lower_bound(data_start) : pending.end();
            if (first == pending.end()) first = pending.begin();
            const uint32_t start = first->first;
            unsigned char data[64 * 512];
            unsigned int count = 0;
            auto at = first;
            while (at != pending.end() && at->first == start + count && count < max_transfer &&
                   region(at->first) == region(start)) {
                memcpy(data + count * 512, at->second.data(), 512); ++count;
                at = pending.erase(at);
            }
            const Result result = sectors_io(start, count, data, true);
            if (result != Result::ok) { invalidate(); return result; }
            remember_metadata(start, count, data);
        }
        return valid ? Result::ok : Result::absent;
    }
    Result end_write() { const Result result = flush_writes(); batching = false; return result; }
    void cancel_write() { if (batching) invalidate(); }

    bool geometry(bool require_fat = true)
    {
        unsigned char cdb[12] = {0x25}, capacity[8] = {}; // READ CAPACITY(10).
        if (command(cdb, sizeof(cdb), capacity, sizeof(capacity)) != Result::ok) return false;
        auto be32 = [](const unsigned char *p) {
            return (uint32_t(p[0]) << 24) | (uint32_t(p[1]) << 16) | (uint32_t(p[2]) << 8) | p[3];
        };
        const uint32_t last = be32(capacity);
        if (be32(capacity + 4) != 512 || last < 639 || last >= 5760) return false;
        blocks = last + 1;
        /* Raw access must work before a filesystem exists. Only established
         * floppy capacities are accepted; no geometry comes from a broken BPB. */
        heads = 2;
        switch (blocks) {
        case 640: cylinders = 40; sectors = 8; break;
        case 720: cylinders = 40; sectors = 9; break;
        case 1280: cylinders = 80; sectors = 8; break;
        case 1440: cylinders = 80; sectors = 9; break;
        case 2400: cylinders = 80; sectors = 15; break;
        case 2880: cylinders = 80; sectors = 18; break;
        case 5760: cylinders = 80; sectors = 36; break;
        default: return false;
        }
        if (require_fat && !fat_geometry()) return false;
        /* MODE SENSE(10), current values, all pages. Fail closed to read-only
         * if the drive cannot report the physical write-protect tab. */
        unsigned char mode[8] = {};
        memset(cdb, 0, sizeof(cdb)); cdb[0] = 0x5a; cdb[2] = 0x3f; cdb[8] = sizeof(mode);
        readonly = command(cdb, sizeof(cdb), mode, sizeof(mode)) != Result::ok ||
                   (mode[3] & 0x80) || !writable_fd;
        return valid;
    }

    bool fat_geometry()
    {
        unsigned char boot[512] = {};
        if (sector(0, boot) != Result::ok) return false;
        auto le16 = [&boot](unsigned int at) { return unsigned(boot[at]) | (unsigned(boot[at + 1]) << 8); };
        const unsigned int cluster = boot[13], reserved = le16(14), fats = boot[16];
        const unsigned int roots = le16(17), total = le16(19), fat_size = le16(22);
        const unsigned int overhead = reserved + fats * fat_size + (roots * 32 + 511) / 512;
        const unsigned int fat_sectors = le16(24), fat_heads = le16(26);
        if (le16(11) != 512 || !cluster || cluster > 128 || (cluster & (cluster - 1)) ||
            !reserved || !fats || fats > 2 || !fat_size || !roots ||
            total != blocks || total <= overhead || (total - overhead) / cluster >= 4085 ||
            fat_sectors != sectors || fat_heads != heads ||
            total % (heads * sectors)) return false;
        if (fat_size * 512 * 2 / 3 < (total - overhead) / cluster + 2) return false;
        root_start = reserved + fats * fat_size; data_start = overhead;
        committed_metadata.clear();
        if (!batching && pending.empty())
            for (const auto &track : tracks)
                if (track.first != UINT32_MAX)
                    remember_metadata(track.first, track.count, track.data.data());
        return valid;
    }
};

inline std::shared_ptr<Device> discover(const char *sys_block = "/sys/class/block",
                                        const char *dev_root = "/dev", bool require_fat = true)
{
    glob_t paths = {};
    std::shared_ptr<Device> result;
    const std::string pattern = std::string(sys_block) + "/sd*";
    if (!glob(pattern.c_str(), 0, nullptr, &paths)) {
        for (size_t i = 0; i < paths.gl_pathc; ++i) {
            const std::string path = paths.gl_pathv[i];
            if (!candidate(path)) continue;
            auto device = std::make_shared<Device>(std::string(dev_root) + path.substr(path.find_last_of('/')));
            if (!device->valid) continue;
            device->configure_io(path);
            /* A new descriptor may acknowledge the drive's initial unit attention.
             * Never retry a write or reuse an invalidated medium. */
            Result present = device->ready();
            if (present == Result::changed) {
                device.reset();
                device = std::make_shared<Device>(std::string(dev_root) + path.substr(path.find_last_of('/')));
                device->configure_io(path);
                present = device->ready();
            }
            if (device->valid && present == Result::ok && device->geometry(require_fat)) {
                device->trace_usb(path);
                result = device;
                break;
            }
        }
    }
    globfree(&paths);
    return result;
}

} // namespace pidos_floppy
#endif
