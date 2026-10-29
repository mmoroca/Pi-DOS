/* USB filesystem mounts for the Pi-DOS appliance. GPL-2.0-or-later. */
#ifndef PI_DOS_USB_STORAGE_LINUX_H
#define PI_DOS_USB_STORAGE_LINUX_H
#include "pidos_usb_storage_io.h"
#include <algorithm>
#include <cerrno>
#include <cctype>
#include <climits>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <strings.h>
#include <blkid/blkid.h>
#include <chrono>
#include <dirent.h>
#include <fcntl.h>
#include <fstream>
#include <map>
#include <memory>
#include <set>
#include <sstream>
#include <string>
#include <sys/mount.h>
#include <sys/stat.h>
#include <sys/sysmacros.h>
#include <unistd.h>
#include <vector>

namespace pidos_storage {
inline std::string line(const std::string &path)
{
    std::ifstream input(path);
    std::string value;
    std::getline(input, value);
    return value;
}
inline std::string canonical(const std::string &path)
{
    char resolved[PATH_MAX];
    return realpath(path.c_str(), resolved) ? resolved : "";
}
inline std::string parent(const std::string &path)
{ return path.substr(0, path.find_last_of('/')); }
inline bool exists(const std::string &path)
{ struct stat st; return !stat(path.c_str(), &st); }
inline std::vector<std::string> children(const std::string &path)
{
    std::vector<std::string> result;
    DIR *dir = opendir(path.c_str());
    if (!dir) return result;
    while (auto entry = readdir(dir))
        if (entry->d_name[0] != '.') result.emplace_back(entry->d_name);
    closedir(dir);
    std::sort(result.begin(), result.end());
    return result;
}
inline std::string physical_disk(const std::string &device)
{
    struct stat st;
    if (stat(device.c_str(), &st) || !S_ISBLK(st.st_mode)) return "";
    std::string path = canonical("/sys/dev/block/" + std::to_string(major(st.st_rdev)) +
                                 ":" + std::to_string(minor(st.st_rdev)));
    return exists(path + "/partition") ? parent(path) : path;
}
inline bool usb_disk(const std::string &path)
{
    if (line(path + "/device/type") != "0") return false;
    std::string model = line(path + "/device/model");
    std::transform(model.begin(), model.end(), model.begin(),
                   [](unsigned char c) { return char(tolower(c)); });
    if (model.find("floppy") != std::string::npos ||
        model.find("usb-fdu") != std::string::npos ||
        model.find("uf000") != std::string::npos) return false;
    for (std::string at = canonical(path + "/device"); !at.empty(); at = parent(at)) {
        if (line(at + "/bInterfaceClass") == "08")
            return line(at + "/bInterfaceSubClass") != "04"; // UFI belongs to A:.
        if (at == parent(at)) break;
    }
    return false;
}

struct Candidate {
    std::string sys, disk, device, number, sequence, geometry;
    unsigned partition = 0;
    bool present() const
    {
        return !sequence.empty() && line(disk + "/diskseq") == sequence &&
               line(sys + "/dev") == number &&
               line(sys + "/start") + ":" + line(sys + "/size") == geometry;
    }
    std::string key() const { return sys + ":" + sequence + ":" + geometry; }
};

/* Read partition metadata from the parent, not from guessed device names.
 * Reject system/hidden containers even when they contain a FAT signature. */
inline bool data_partition(const Candidate &candidate)
{
    if (!candidate.partition) return true;
    const std::string device = "/dev/" + candidate.disk.substr(candidate.disk.find_last_of('/') + 1);
    blkid_probe probe = blkid_new_probe_from_filename(device.c_str());
    if (!probe) return false;
    blkid_probe_enable_superblocks(probe, 0);
    blkid_probe_enable_partitions(probe, 1);
    bool allowed = false;
    if (blkid_do_safeprobe(probe) == 0) {
        blkid_partlist parts = blkid_probe_get_partitions(probe);
        blkid_partition part = parts ? blkid_partlist_get_partition_by_partno(parts, candidate.partition) : nullptr;
        if (part) {
            allowed = true;
            const char *type = blkid_partition_get_type_string(part);
            if (type) {
                const std::string guid(type);
                static const char *special[] = {
                    "c12a7328-f81f-11d2-ba4b-00a0c93ec93b", // EFI
                    "e3c9e316-0b5c-4db8-817d-f92df00215ae", // MS reserved
                    "de94bba4-06d1-4d40-a16a-bfd50179d6ac", // Windows recovery
                    "a19d880f-05fc-4d3b-a006-743f0f84911e", // Linux RAID
                    "e6d6d379-f507-44c2-a23c-238f2a3df928", // Linux LVM
                    "0657fd6d-a4ab-43c4-84e5-0933c84b4f4f"  // swap
                };
                for (auto excluded : special) if (!strcasecmp(guid.c_str(), excluded)) allowed = false;
                const auto flags = blkid_partition_get_flags(part);
                if (flags & (3ULL | (1ULL << 62) | (1ULL << 63))) allowed = false;
            } else {
                switch (blkid_partition_get_type(part)) {
                case 0x05: case 0x0f: case 0x85: // extended containers
                case 0x11: case 0x14: case 0x16: case 0x17:
                case 0x1b: case 0x1c: case 0x1e: // hidden FAT/NTFS
                case 0x27: case 0x82: case 0x8e: case 0xef: case 0xfd:
                    allowed = false;
                }
            }
        }
    }
    blkid_free_probe(probe);
    return allowed;
}

inline std::vector<Candidate> discover()
{
    std::vector<Candidate> result;
    const char *system = getenv("PI_DOS_DATA_DEVICE");
    const std::string system_disk = physical_disk(system ? system : "/dev/mmcblk0p2");
    const auto names = children("/sys/class/block");
    std::set<std::string> partitioned;
    for (const auto &name : names) {
        const std::string sys = canonical("/sys/class/block/" + name);
        if (exists(sys + "/partition")) partitioned.insert(parent(sys));
    }
    for (const auto &name : names) {
        Candidate candidate;
        candidate.sys = canonical("/sys/class/block/" + name);
        const std::string index = line(candidate.sys + "/partition");
        candidate.partition = index.empty() ? 0 : unsigned(strtoul(index.c_str(), nullptr, 10));
        candidate.disk = candidate.partition ? parent(candidate.sys) : candidate.sys;
        if (candidate.disk.empty() || candidate.disk == system_disk || !usb_disk(candidate.disk)) continue;
        if (!candidate.partition && partitioned.count(candidate.disk)) continue;
        candidate.device = "/dev/" + name;
        candidate.number = line(candidate.sys + "/dev");
        candidate.sequence = line(candidate.disk + "/diskseq");
        candidate.geometry = line(candidate.sys + "/start") + ":" + line(candidate.sys + "/size");
        if (candidate.present()) result.push_back(candidate);
    }
    std::sort(result.begin(), result.end(), [](const Candidate &a, const Candidate &b) {
        return a.disk == b.disk ? a.partition < b.partition : a.disk < b.disk;
    });
    return result;
}

struct Volume {
    Candidate candidate;
    std::string path, label, filesystem;
    bool active = true, readonly = false;
    int fd = -1;
    bool present() const { return active && candidate.present(); }
    bool flush() const
    { return present() && (readonly || (fd >= 0 && !host_io([this] { return syncfs(fd); }))); }
    void detach()
    {
        active = false;
        if (fd >= 0) { close(fd); fd = -1; }
        if (!path.empty()) {
            // Retired mounts have unique paths; stale descriptors never reach a new volume.
            if (umount2(path.c_str(), MNT_DETACH) && errno != EINVAL && errno != ENOENT)
                fprintf(stderr, "Pi-DOS USB storage: detach %s: %s\n", path.c_str(), strerror(errno));
            rmdir(path.c_str());
            path.clear();
        }
    }
    ~Volume() { detach(); }
};

inline std::shared_ptr<Volume> mount_volume(const Candidate &candidate, unsigned long generation)
{
    if (!data_partition(candidate)) return {};
    struct stat node;
    if (stat(candidate.device.c_str(), &node) || !S_ISBLK(node.st_mode)) return {};
    // Do not claim volumes already mounted by any other subsystem.
    std::ifstream mounts("/proc/self/mountinfo");
    std::string record;
    while (std::getline(mounts, record)) {
        std::istringstream fields(record);
        std::string id, parent_id, number;
        fields >> id >> parent_id >> number;
        if (number == candidate.number) return {};
    }
    blkid_probe probe = blkid_new_probe_from_filename(candidate.device.c_str());
    if (!probe) return {};
    blkid_probe_enable_partitions(probe, 1);
    const char *type = nullptr, *label = nullptr, *table = nullptr;
    std::shared_ptr<Volume> volume;
    if (blkid_do_safeprobe(probe) == 0 &&
        !blkid_probe_lookup_value(probe, "TYPE", &type, nullptr) &&
        (!strcmp(type, "vfat") || !strcmp(type, "exfat")) &&
        blkid_probe_lookup_value(probe, "PTTYPE", &table, nullptr) != 0) {
        volume = std::make_shared<Volume>();
        volume->candidate = candidate;
        volume->filesystem = type;
        if (!blkid_probe_lookup_value(probe, "LABEL", &label, nullptr)) volume->label = label;
    }
    blkid_free_probe(probe);
    if (!volume || !candidate.present()) return {};
    const std::string root = "/run/pi-dos-usb";
    if (mkdir(root.c_str(), 0700) && errno != EEXIST) return {};
    volume->path = root + "/" + std::to_string(generation);
    if (mkdir(volume->path.c_str(), 0700)) { volume->path.clear(); return {}; }
    const unsigned long flags = MS_NODEV | MS_NOSUID | MS_NOEXEC | MS_NOATIME | MS_SYNCHRONOUS;
    const char *options = volume->filesystem == "vfat" ? "utf8=1,errors=remount-ro" : "errors=remount-ro";
    if (mount(candidate.device.c_str(), volume->path.c_str(), volume->filesystem.c_str(), flags, options)) {
        if (mount(candidate.device.c_str(), volume->path.c_str(), volume->filesystem.c_str(), flags | MS_RDONLY, options)) {
            fprintf(stderr, "Pi-DOS USB storage: cannot mount %s (%s): %s\n",
                    candidate.device.c_str(), volume->filesystem.c_str(), strerror(errno));
            return {};
        }
        volume->readonly = true;
    }
    volume->fd = open(volume->path.c_str(), O_RDONLY | O_DIRECTORY | O_CLOEXEC);
    struct stat mounted;
    if (volume->fd < 0 || fstat(volume->fd, &mounted) || mounted.st_dev != node.st_rdev ||
        !candidate.present()) return {};
    return volume;
}
} // namespace pidos_storage
#endif
