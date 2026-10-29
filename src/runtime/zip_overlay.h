/* Persistent ZIP copy-on-write storage. Included after the PhysFS API.
 * GPL-2.0-or-later. No PhysFS global write directory is used. */
#ifndef PI_DOS_ZIP_OVERLAY_H
#define PI_DOS_ZIP_OVERLAY_H
#include <algorithm>
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <dirent.h>
#include <fcntl.h>
#include <set>
#include <string>
#include <sys/stat.h>
#include <unistd.h>
#include <utime.h>
#include <vector>

namespace pidos_zip {
class Overlay {
    std::string archive, root, mount;
    struct stat archive_id = {}, root_id = {};
    std::set<std::string> deleted;
    bool ro;

    static bool same(const struct stat &a, const struct stat &b)
    { return a.st_dev == b.st_dev && a.st_ino == b.st_ino; }
    bool safe(const std::string &path) const
    {
        if (!valid(path)) { errno = EACCES; return false; }
        std::string current = root;
        size_t start = 0;
        while (start < path.size()) {
            const size_t end = path.find('/', start);
            current += "/" + path.substr(start, end - start);
            struct stat st;
            if (!lstat(current.c_str(), &st)) {
                if (S_ISLNK(st.st_mode)) { errno = EACCES; return false; }
            } else if (errno == ENOENT) return true;
            else return false;
            if (end == std::string::npos) break;
            start = end + 1;
        }
        return true;
    }
    bool parents(const std::string &path)
    {
        size_t end = 0;
        while ((end = path.find('/', end)) != std::string::npos) {
            const std::string part = path.substr(0, end++);
            PHYSFS_Stat st;
            if (!stat(part, st) || st.filetype != PHYSFS_FILETYPE_DIRECTORY || !safe(part))
                return false;
            if (::mkdir(host(part).c_str(), 0777) && errno != EEXIST) return false;
        }
        return true;
    }
    bool mark(const std::string &path)
    {
        const std::string key = fold(path);
        if (deleted.count(key)) return true;
        deleted.insert(key);
        const std::string temporary = root + "/PFSINFO/DELETE.TMP";
        const int fd = ::open(temporary.c_str(), O_WRONLY | O_CREAT | O_TRUNC | O_NOFOLLOW, 0600);
        FILE *out = fd < 0 ? nullptr : fdopen(fd, "wb");
        if (fd >= 0 && !out) ::close(fd);
        bool ok = out != nullptr;
        if (out) {
            for (const auto &entry : deleted) {
                for (unsigned char c : entry)
                    if (fprintf(out, "%02X", c) != 2) ok = false;
                if (fputc('\n', out) == EOF) ok = false;
            }
            if (fflush(out) || fsync(fileno(out))) ok = false;
            if (fclose(out)) ok = false;
        }
        if (ok) ok = !::rename(temporary.c_str(), (root + "/PFSINFO/DELETE.DAT").c_str());
        if (!ok) { deleted.erase(key); ::unlink(temporary.c_str()); }
        return ok;
    }
public:
    Overlay(const std::string &zip, const std::string &directory,
            const std::string &mountpoint, bool readonly)
        : archive(zip), root(directory), mount(mountpoint), ro(readonly) {}
    static std::string fold(std::string value)
    {
        for (char &c : value) if (c >= 'a' && c <= 'z') c -= 'a' - 'A';
        return value;
    }
    static bool valid(const std::string &path)
    {
        if (path.empty()) return true;
        size_t start = 0;
        while (start < path.size()) {
            size_t end = path.find('/', start);
            const std::string part = path.substr(start, end - start);
            if (part.empty() || part == "." || part == ".." ||
                fold(part) == "PFSINFO" || part.find('\\') != std::string::npos ||
                part.find(':') != std::string::npos) return false;
            if (end == std::string::npos) return true;
            start = end + 1;
        }
        return false;
    }
    std::string host(const std::string &path) const { return root + "/" + path; }
    std::string virtualPath(const std::string &path) const { return mount + "/" + path; }
    bool init()
    {
        if (::stat(archive.c_str(), &archive_id)) return false;
        if (::lstat(root.c_str(), &root_id)) {
            if (errno != ENOENT) return false;
            if (ro) return true;
            if (::mkdir(root.c_str(), 0777) || ::lstat(root.c_str(), &root_id)) return false;
        }
        if (!S_ISDIR(root_id.st_mode)) { errno = ENOTDIR; return false; }
        const std::string metadata = root + "/PFSINFO";
        struct stat st;
        if (::lstat(metadata.c_str(), &st)) {
            if (errno != ENOENT) return false;
            if (ro) return true;
            if (::mkdir(metadata.c_str(), 0777)) return false;
        } else if (!S_ISDIR(st.st_mode)) { errno = ENOTDIR; return false; }
        const std::string manifest = metadata + "/DELETE.DAT";
        if (!::lstat(manifest.c_str(), &st) && !S_ISREG(st.st_mode)) return false;
        FILE *in = fopen(manifest.c_str(), "rb");
        if (!in) return errno == ENOENT;
        std::string entry;
        int c;
        bool ok = true;
        while ((c = fgetc(in)) != EOF) {
            if (c == '\n') {
                std::string decoded;
                if (entry.size() % 2) { ok = false; break; }
                for (size_t i = 0; i < entry.size(); i += 2) {
                    auto hex = [](char h) -> int {
                        return h >= '0' && h <= '9' ? h - '0' :
                               h >= 'A' && h <= 'F' ? h - 'A' + 10 : -1;
                    };
                    const int a = hex(entry[i]), b = hex(entry[i + 1]);
                    if (a < 0 || b < 0 || !(a * 16 + b)) { ok = false; break; }
                    decoded += char(a * 16 + b);
                }
                if (!ok || decoded.empty() || !valid(decoded)) { ok = false; break; }
                deleted.insert(decoded);
                entry.clear();
            } else if (entry.size() < 8192) entry += char(c);
            else { ok = false; break; }
        }
        if (ferror(in) || !entry.empty()) ok = false;
        fclose(in);
        if (!ok) errno = EINVAL;
        return ok;
    }
    bool ready() const
    {
        struct stat a, b;
        if (::stat(archive.c_str(), &a) || !same(a, archive_id) ||
            a.st_size != archive_id.st_size || a.st_mtime != archive_id.st_mtime) {
            errno = EIO; return false;
        }
        if (root_id.st_ino && (::lstat(root.c_str(), &b) || !same(b, root_id))) {
            errno = EIO; return false;
        }
        return true;
    }
    bool writable() const
    { if (ro) { errno = EROFS; return false; } return ready(); }
    bool upper(const std::string &path) const
    {
        struct stat st;
        return safe(path) && !::lstat(host(path).c_str(), &st);
    }
    bool hidden(const std::string &path) const
    {
        std::string key = fold(path);
        for (;;) {
            if (deleted.count(key)) return true;
            const size_t slash = key.rfind('/');
            if (slash == std::string::npos) return false;
            key.resize(slash);
        }
    }
    bool stat(const std::string &path, PHYSFS_Stat &st) const
    {
        if (!ready() || !safe(path)) return false;
        if (hidden(path) && !upper(path)) { errno = ENOENT; return false; }
        if (!PHYSFS_stat(virtualPath(path).c_str(), &st)) { errno = ENOENT; return false; }
        return true;
    }
    std::vector<std::string> list(const std::string &path) const
    {
        std::vector<std::string> result;
        PHYSFS_Stat st;
        if (!stat(path, st) || st.filetype != PHYSFS_FILETYPE_DIRECTORY) return result;
        char **entries = PHYSFS_enumerateFiles(virtualPath(path).c_str());
        if (!entries) return result;
        for (char **entry = entries; *entry; ++entry) {
            const std::string child = path.empty() ? *entry : path + "/" + *entry;
            if (stat(child, st)) result.emplace_back(*entry);
        }
        PHYSFS_freeList(entries);
        return result;
    }
    bool copyUp(const std::string &path)
    {
        if (!writable() || path.empty() || !safe(path)) return false;
        PHYSFS_Stat st;
        if (!stat(path, st)) return false;
        if (upper(path)) return true;
        if (!parents(path)) return false;
        if (st.filetype == PHYSFS_FILETYPE_DIRECTORY)
            return !::mkdir(host(path).c_str(), 0777);
        if (st.filetype != PHYSFS_FILETYPE_REGULAR) { errno = EACCES; return false; }
        PHYSFS_File *in = PHYSFS_openRead(virtualPath(path).c_str());
        if (!in) { errno = EIO; return false; }
        std::string temporary = root + "/PFSINFO/CPXXXXXX";
        std::vector<char> pattern(temporary.begin(), temporary.end());
        pattern.push_back(0);
        int fd = mkstemp(pattern.data());
        FILE *out = fd < 0 ? nullptr : fdopen(fd, "wb");
        bool ok = out != nullptr;
        if (fd >= 0 && !out) ::close(fd);
        char buffer[65536];
        PHYSFS_sint64 count = 0;
        while (ok && (count = PHYSFS_readBytes(in, buffer, sizeof(buffer))) > 0)
            ok = fwrite(buffer, 1, size_t(count), out) == size_t(count);
        if (count < 0) ok = false;
        if (!PHYSFS_close(in)) ok = false;
        if (out) {
            if (fflush(out) || fsync(fileno(out))) ok = false;
            if (fclose(out)) ok = false;
        }
        if (ok) {
            struct utimbuf times = {time_t(st.modtime), time_t(st.modtime)};
            ::utime(pattern.data(), &times);
            ok = !::rename(pattern.data(), host(path).c_str());
        }
        if (!ok) { ::unlink(pattern.data()); errno = EIO; }
        return ok;
    }
    bool prepareCreate(const std::string &path)
    { return writable() && !path.empty() && safe(path) && parents(path); }
    bool remove(const std::string &path, bool directory)
    {
        if (!writable() || path.empty()) return false;
        PHYSFS_Stat st;
        if (!stat(path, st) || (st.filetype == PHYSFS_FILETYPE_DIRECTORY) != directory)
            return false;
        if (directory && !list(path).empty()) { errno = ENOTEMPTY; return false; }
        if (!mark(path)) return false;
        if (!upper(path)) return true;
        return directory ? !::rmdir(host(path).c_str()) : !::unlink(host(path).c_str());
    }
    bool copyTree(const std::string &path)
    {
        PHYSFS_Stat st;
        if (!stat(path, st) || !copyUp(path)) return false;
        if (st.filetype == PHYSFS_FILETYPE_DIRECTORY)
            for (const auto &child : list(path))
                if (!copyTree(path + "/" + child)) return false;
        return true;
    }
    bool rename(const std::string &from, const std::string &to)
    {
        if (!writable() || from.empty() || to.empty() || !safe(to)) return false;
        if (fold(from) == fold(to) || fold(to).find(fold(from) + "/") == 0) {
            errno = EACCES; return false;
        }
        PHYSFS_Stat st;
        if (stat(to, st)) { errno = EEXIST; return false; }
        if (!parents(to) || !copyTree(from) || !mark(from)) return false;
        return !::rename(host(from).c_str(), host(to).c_str());
    }
};
} // namespace pidos_zip
#endif
