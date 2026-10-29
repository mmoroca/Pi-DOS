/* DOS sharing and record locks, independent of host process locks.
 * GPL-2.0-or-later. All calls run on the emulator thread. */
#ifndef PI_DOS_SHARING_H
#define PI_DOS_SHARING_H
#include <algorithm>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace pidos_sharing {
inline uint64_t next_media_id()
{ static uint64_t sequence = 0; return ++sequence; }

struct Key {
    uint64_t media = 0;
    std::string path;
    bool operator==(const Key &other) const
    { return media == other.media && path == other.path; }
};
struct Range {
    uint64_t begin, end;
    uint16_t owner;
};
struct Open {
    Key key;
    uint8_t access, mode;
    uint16_t fcb_owner = 0;
    std::vector<Range> locks;
};

class Table {
    std::vector<std::weak_ptr<Open>> opens;
    static constexpr uint64_t limit = uint64_t(1) << 32;
    void prune()
    {
        opens.erase(std::remove_if(opens.begin(), opens.end(),
                    [](const std::weak_ptr<Open> &p) { return p.expired(); }), opens.end());
    }
    static bool overlaps(uint64_t a, uint64_t b, const Range &range)
    { return a < range.end && range.begin < b; }
    static uint8_t denied(uint8_t mode)
    { return mode == 1 ? 3 : mode == 2 ? 2 : mode == 3 ? 1 : 0; }
    static bool compatible(const Open &old, uint8_t access, uint8_t mode, bool compat_read)
    {
        // Legacy compatibility opens can coexist with compatibility opens.
        // Mixing them with explicit sharing follows DOS's read-only cases.
        if (!old.mode || !mode) {
            if (!old.mode && !mode) return true;
            if (!old.mode)
                return compat_read && old.access == 1 && access == 1 && (mode == 2 || mode == 4);
            return old.access == 1 && ((old.mode == 4 && (access != 1 || compat_read)) ||
                                      (compat_read && old.mode == 2 && access == 1));
        }
        return !(denied(old.mode) & access) && !(denied(mode) & old.access);
    }
public:
    static int decode(uint8_t flags, uint8_t &access, uint8_t &mode)
    {
        const auto requested = flags & 15;
        mode = (flags >> 4) & 7;
        if ((requested > 2 && requested != 4) || mode > 4) return 12;
        access = requested == 1 ? 2 : requested == 2 ? 3 : 1;
        return 0;
    }
    int check_open(const Key &key, uint8_t flags, bool compat_read = false)
    {
        uint8_t access = 0, mode = 0;
        if (const int error = decode(flags, access, mode)) return error;
        prune();
        for (const auto &weak : opens) if (auto old = weak.lock())
            if (old->key == key && !compatible(*old, access, mode, compat_read)) return 32;
        return 0;
    }
    std::shared_ptr<Open> opened(const Key &key, uint8_t flags, uint16_t fcb_owner = 0)
    {
        uint8_t access = 0, mode = 0;
        decode(flags, access, mode);
        auto result = std::make_shared<Open>();
        result->key = key; result->access = access; result->mode = mode;
        result->fcb_owner = fcb_owner;
        prune(); opens.push_back(result);
        return result;
    }
    int access(const Key &key, uint16_t owner, uint64_t begin, uint64_t end)
    {
        if (end > limit || begin > end) return 33;
        if (begin == end) return 0;
        prune();
        for (const auto &weak : opens) if (auto file = weak.lock())
            if (file->key == key) for (const auto &range : file->locks)
                if (range.owner != owner && overlaps(begin, end, range)) return 33;
        return 0;
    }
    int destructive(const Key &key, bool directory = false)
    {
        prune();
        const std::string prefix = key.path + "\\";
        for (const auto &weak : opens) if (auto file = weak.lock())
            if (file->key.media == key.media && (file->key.path == key.path ||
                (directory && file->key.path.compare(0, prefix.size(), prefix) == 0))) return 32;
        return 0;
    }
    int lock(const std::shared_ptr<Open> &file, uint16_t owner, uint8_t mode,
             uint32_t position, uint32_t length)
    {
        if (mode > 1) return 1;
        const uint64_t end = uint64_t(position) + length;
        if (end > limit) return 33;
        if (!length) return 0; // DOS zero-length requests do not mean "to EOF".
        if (mode) {
            const auto at = std::find_if(file->locks.begin(), file->locks.end(),
                [&](const Range &r) { return r.owner == owner && r.begin == position && r.end == end; });
            if (at == file->locks.end()) return 33;
            file->locks.erase(at); return 0;
        }
        prune();
        size_t count = 0;
        for (const auto &weak : opens) if (auto other = weak.lock()) {
            count += other->locks.size();
            if (other->key == file->key) for (const auto &range : other->locks)
                if (overlaps(position, end, range)) return 33;
        }
        if (count >= 4096) return 36;
        file->locks.push_back({position, end, owner}); return 0;
    }
    void release_owner(uint16_t owner)
    {
        prune();
        for (const auto &weak : opens) if (auto file = weak.lock()) {
            auto &locks = file->locks;
            locks.erase(std::remove_if(locks.begin(), locks.end(),
                        [owner](const Range &r) { return r.owner == owner; }), locks.end());
        }
    }
    void clear() { opens.clear(); }
};
} // namespace pidos_sharing
#endif
