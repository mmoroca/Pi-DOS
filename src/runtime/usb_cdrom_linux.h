/* Read-only USB MMC CD transport. Distributed with the DOSBox-X GPL runtime. */
#ifndef PI_DOS_USB_CDROM_LINUX_H
#define PI_DOS_USB_CDROM_LINUX_H

#include <algorithm>
#include <array>
#include <cerrno>
#include <chrono>
#include <climits>
#include <cstdint>
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <fcntl.h>
#include <glob.h>
#include <linux/cdrom.h>
#include <linux/fs.h>
#include <scsi/sg.h>
#include <string>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <unistd.h>
#include <vector>

namespace pidos_cdrom {

inline std::string line(const std::string &path)
{
    char value[256] = {};
    FILE *file = fopen(path.c_str(), "r");
    if (file) { if (!fgets(value, sizeof(value), file)) value[0] = 0; fclose(file); }
    value[strcspn(value, "\r\n")] = 0;
    return value;
}

inline std::string identity(const std::string &path)
{
    char resolved[4096];
    return realpath((path + "/device").c_str(), resolved) ? resolved : "";
}

inline bool candidate(const std::string &path)
{
    if (line(path + "/device/type") != "5" || !line(path + "/partition").empty()) return false;
    std::string parent = identity(path);
    while (!parent.empty()) {
        if (line(parent + "/bInterfaceClass") == "08") return true;
        const auto slash = parent.find_last_of('/');
        if (slash == std::string::npos) break;
        parent.resize(slash);
    }
    return false;
}

inline uint32_t be32(const uint8_t *p)
{ return uint32_t(p[0]) << 24 | uint32_t(p[1]) << 16 | uint32_t(p[2]) << 8 | p[3]; }
inline void put32(uint8_t *p, uint32_t n)
{ p[0] = n >> 24; p[1] = n >> 16; p[2] = n >> 8; p[3] = n; }

struct Track { uint8_t number, attr; uint32_t start; };

/* Used only by the player's I/O worker. No Linux filesystem mount, media
 * writes, analogue playback or dependence on SDL's boot-time CD enumeration. */
class Device {
    int fd = -1;
    std::string sys, id, diskseq;
    std::chrono::steady_clock::time_point next_scan = {};
    bool trace = false;
    enum class Result { ok, attention, absent, error };

    void invalidate()
    {
        ++generation; present = false; tracks.clear(); leadout = 0;
    }
    void disconnect()
    {
        if (fd >= 0) { close(fd); fd = -1; invalidate(); }
        path.clear(); sys.clear(); id.clear(); diskseq.clear();
    }
    Result command(uint8_t *cdb, unsigned int size, void *data = nullptr, unsigned int bytes = 0, bool short_ok = false)
    {
        if (fd < 0) return Result::absent;
        uint8_t sense[32] = {};
        sg_io_hdr_t io = {};
        io.interface_id = 'S'; io.cmdp = cdb; io.cmd_len = size;
        io.dxfer_direction = bytes ? SG_DXFER_FROM_DEV : SG_DXFER_NONE;
        io.dxferp = data; io.dxfer_len = bytes;
        io.sbp = sense; io.mx_sb_len = sizeof(sense); io.timeout = 3000;
        const int result = ioctl(fd, SG_IO, &io);
        if (result < 0) {
            if (trace) fprintf(stderr, "Pi-DOS USB CD-ROM: op=%02x errno=%d\n", cdb[0], errno);
            if (errno == ENODEV || errno == ENXIO || errno == EBADF) disconnect();
            return Result::error;
        }
        if (io.host_status) {
            if (trace) fprintf(stderr, "Pi-DOS USB CD-ROM: op=%02x host_status=%u\n", cdb[0], io.host_status);
            disconnect(); return Result::error;
        }
        if (!io.status && !io.driver_status && (io.resid == 0 ||
            (short_ok && io.resid >= 0 && unsigned(io.resid) <= bytes))) return Result::ok;
        const unsigned response = sense[0] & 0x7f;
        const unsigned key = response == 0x72 || response == 0x73 ? sense[1] & 15 : sense[2] & 15;
        if (trace) fprintf(stderr, "Pi-DOS USB CD-ROM: op=%02x status=%u key=%x resid=%d\n",
                           cdb[0], io.status, key, io.resid);
        if (key == 6) { invalidate(); return Result::attention; }
        if (key == 2) { if (present) invalidate(); return Result::absent; }
        return Result::error;
    }
    bool discover()
    {
        const auto now = std::chrono::steady_clock::now();
        if (now < next_scan) return false;
        next_scan = now + std::chrono::milliseconds(500);
        glob_t names = {};
        if (glob((sysroot + "/*").c_str(), 0, nullptr, &names) != 0) { globfree(&names); return false; }
        for (size_t i = 0; i < names.gl_pathc; ++i) {
            const std::string entry = names.gl_pathv[i];
            if (!candidate(entry)) continue;
            const std::string node = devroot + entry.substr(entry.find_last_of('/'));
            const int opened = open(node.c_str(), O_RDONLY | O_NONBLOCK | O_CLOEXEC);
            if (opened < 0) continue;
            struct stat st = {};
            if (fstat(opened, &st) || !S_ISBLK(st.st_mode)) { close(opened); continue; }
            fd = opened; sys = entry; path = node; id = identity(entry);
            diskseq = line(sys + "/diskseq");
            invalidate();
            if (trace) fprintf(stderr, "Pi-DOS USB CD-ROM: device=%s model=%s\n",
                               path.c_str(), line(sys + "/device/model").c_str());
            break;
        }
        globfree(&names);
        return fd >= 0;
    }
    bool toc()
    {
        uint8_t cdb[10] = {0x43}; // READ TOC, LBA addresses, tracks plus lead-out
        std::array<uint8_t, 804> data = {};
        cdb[7] = data.size() >> 8; cdb[8] = data.size() & 255;
        if (command(cdb, sizeof(cdb), data.data(), data.size(), true) != Result::ok) return false;
        const unsigned bytes = (unsigned(data[0]) << 8 | data[1]) + 2;
        if (bytes < 20 || bytes > data.size() || (bytes - 4) % 8 ||
            data[2] < 1 || data[3] > 99 || data[2] > data[3]) return false;
        std::vector<Track> found;
        uint32_t end = 0;
        for (unsigned off = 4; off + 8 <= bytes; off += 8) {
            const uint8_t number = data[off + 2];
            const uint32_t start = be32(&data[off + 4]);
            if (number == 0xaa) { end = start; break; }
            if (number != data[2] + found.size() || number > data[3] ||
                (!found.empty() && start <= found.back().start)) return false;
            found.push_back({number, uint8_t((data[off + 1] & 15) << 4), start});
        }
        if (found.size() != unsigned(data[3] - data[2] + 1) || end <= found.back().start) return false;
        tracks = std::move(found); leadout = end;
        return true;
    }
public:
    const std::string sysroot, devroot;
    std::string path;
    uint64_t generation = 1;
    bool present = false, tray_open = false;
    uint32_t leadout = 0;
    std::vector<Track> tracks;

    explicit Device(std::string sys_root = "/sys/class/block", std::string dev_root = "/dev")
        : sysroot(std::move(sys_root)), devroot(std::move(dev_root))
    { const char *value = getenv("PI_DOS_DIAGNOSTICS"); trace = value && !strcmp(value, "1"); }
    ~Device() { disconnect(); }
    Device(const Device &) = delete;
    Device &operator=(const Device &) = delete;

    bool ready()
    {
        if (fd >= 0 && (identity(sys) != id || !candidate(sys) || line(sys + "/diskseq") != diskseq)) disconnect();
        if (fd < 0 && !discover()) return false;
        // The kernel's independent media-change latch also catches a swap when
        // a different client consumed unit attention. Never compare disc length.
        if (ioctl(fd, CDROM_MEDIA_CHANGED, CDSL_CURRENT) > 0) invalidate();
        uint8_t cdb[6] = {}; // TEST UNIT READY; retry only readiness, never reads
        Result result = Result::error;
        for (unsigned retry = 0; retry < 3; ++retry) {
            result = command(cdb, sizeof(cdb));
            if (result != Result::attention) break;
        }
        if (result != Result::ok) {
            if (present) invalidate();
            // Query tray details only after our own readiness check: the
            // kernel's status ioctl itself sends TUR and can consume attention.
            tray_open = fd >= 0 && ioctl(fd, CDROM_DRIVE_STATUS, CDSL_CURRENT) == CDS_TRAY_OPEN;
            return false;
        }
        tray_open = false;
        if (!present) {
            if (!toc()) return false;
            present = true; ++generation;
            if (trace) fprintf(stderr, "Pi-DOS USB CD-ROM: media=%llu tracks=%zu sectors=%u\n",
                               (unsigned long long)generation, tracks.size(), leadout);
        }
        diskseq = line(sys + "/diskseq");
        return true;
    }
    const Track *track(uint32_t sector) const
    {
        if (sector >= leadout) return nullptr;
        for (size_t i = tracks.size(); i > 0; --i)
            if (sector >= tracks[i - 1].start) return &tracks[i - 1];
        return nullptr;
    }
    bool audio_range(uint32_t start, uint32_t count) const
    {
        if (!count || start >= leadout || count > leadout - start) return false;
        const Track *first = track(start);
        if (!first || (first->attr & 0x40)) return false;
        for (const auto &t : tracks)
            if (t.start > start && t.start < start + count && (t.attr & 0x40)) return false;
        return true;
    }
    bool read(void *buffer, bool raw, uint32_t sector, uint32_t count, bool audio = false)
    {
        if (!count) return true;
        if (!ready() || sector >= leadout || count > leadout - sector || count > 32 ||
            (audio && !audio_range(sector, count))) return false;
        const uint64_t epoch = generation;
        uint8_t cdb[12] = {};
        if (raw || audio) {
            cdb[0] = 0xbe; cdb[1] = audio ? 4 : 0; // READ CD, CD-DA or any sector type
            put32(cdb + 2, sector); cdb[8] = count;
            const auto *entry = track(sector);
            cdb[9] = audio || (entry && !(entry->attr & 0x40)) ? 0x10 : 0xf8;
        } else {
            cdb[0] = 0x28; put32(cdb + 2, sector); cdb[8] = count; // READ(10)
        }
        const unsigned bytes = count * (raw || audio ? 2352 : 2048);
        return command(cdb, raw || audio ? 12 : 10, buffer, bytes) == Result::ok && epoch == generation;
    }
    bool eject(bool unload)
    {
        if (fd < 0 && !discover()) return false;
        if (ioctl(fd, unload ? CDROMEJECT : CDROMCLOSETRAY, 0) < 0) return false;
        invalidate(); tray_open = unload;
        return true;
    }
    bool upc(char *value)
    {
        if (!ready()) return false;
        struct cdrom_mcn mcn = {};
        if (ioctl(fd, CDROM_GET_MCN, &mcn) < 0) return false;
        memcpy(value, mcn.medium_catalog_number, 13); value[13] = 0;
        return true;
    }
};

} // namespace pidos_cdrom
#endif
