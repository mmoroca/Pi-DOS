/* Native FAT/exFAT DOS attributes. GPL-2.0-or-later. */
#ifndef PI_DOS_ATTRIBUTES_LINUX_H
#define PI_DOS_ATTRIBUTES_LINUX_H
#if defined(LINUX)
#include <cerrno>
#include <cstdint>
#include <fcntl.h>
#include <linux/msdos_fs.h>
#include <sys/ioctl.h>
#include <unistd.h>

namespace pidos_attributes {
constexpr uint32_t mutable_bits = 0x27; // Read-only, hidden, system, archive.

inline bool get(int fd, uint16_t &value)
{
    uint32_t attributes = 0;
    if (ioctl(fd, FAT_IOCTL_GET_ATTRIBUTES, &attributes)) return false;
    value = uint16_t(attributes & 0x3f);
    return true;
}

inline bool set(int fd, uint16_t value)
{
    uint32_t attributes = 0;
    if (ioctl(fd, FAT_IOCTL_GET_ATTRIBUTES, &attributes)) return false;
    attributes = (attributes & ~mutable_bits) | (value & mutable_bits);
    if (ioctl(fd, FAT_IOCTL_SET_ATTRIBUTES, &attributes)) return false;
    // The ioctl dirties the inode; do not report success before it is durable.
    return fsync(fd) == 0;
}

template<class Operation> inline bool with_file(const char *path, Operation operation)
{
    const int fd = open(path, O_RDONLY | O_CLOEXEC | O_NOFOLLOW | O_NONBLOCK);
    if (fd < 0) return false;
    const bool result = operation(fd);
    const int error = errno;
    close(fd);
    errno = error;
    return result;
}
inline bool get(const char *path, uint16_t &value)
{ return with_file(path, [&](int fd) { return get(fd, value); }); }
inline bool set(const char *path, uint16_t value)
{ return with_file(path, [&](int fd) { return set(fd, value); }); }
} // namespace pidos_attributes
#endif
#endif
