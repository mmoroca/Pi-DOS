/* Included by drive_physfs.cpp after its PhysFS implementation. GPL-2.0-or-later. */
#ifndef PI_DOS_ZIP_DRIVE_H
#define PI_DOS_ZIP_DRIVE_H
#if defined(LINUX) && !defined(OSFREE)
// The embedded PhysFS implementation defines this private macro, which would
// otherwise rewrite std::allocator in standard headers below.
#undef allocator
#include <memory>
#include "pidos_zip_overlay.h"
#include "pidos_dos_attributes_linux.h"

namespace pidos_zip {
static bool error()
{
    DOS_SetError(errno == ENOENT ? DOSERR_FILE_NOT_FOUND :
                 errno == EROFS ? DOSERR_WRITE_PROTECTED :
                 errno == EIO ? 21 : DOSERR_ACCESS_DENIED);
    return false;
}

class File final : public DOS_File {
    std::shared_ptr<Overlay> storage;
    DOS_File *inner;
    localDrive *upper;
    std::string path;
    bool from_archive;
    bool refresh()
    {
        if (!storage->ready()) return error();
        if (from_archive && storage->upper(path)) {
            uint32_t position = 0;
            DOS_File *replacement = nullptr;
            if (!inner->Seek(&position, DOS_SEEK_CUR) ||
                !upper->FileOpen(&replacement, path.c_str(), flags)) return false;
            replacement->refCtr = 1;
            if (!replacement->Seek(&position, DOS_SEEK_SET)) {
                replacement->Close(); delete replacement; return false;
            }
            inner->Close(); delete inner;
            inner = replacement; from_archive = false;
        }
        return inner->SharingRefresh();
    }
public:
    File(std::shared_ptr<Overlay> owner, DOS_File *file, const char *filename,
         localDrive *writable, const std::string &relative, bool archived)
        : storage(owner), inner(file), upper(writable), path(relative), from_archive(archived)
    {
        inner->refCtr = 1;
        flags = inner->flags; date = inner->date; time = inner->time; attr = inner->attr;
        SetName(filename); open = true;
    }
    bool SharingReady() override { return storage->ready() ? true : error(); }
    bool SharingRefresh() override { return refresh(); }
    bool Read(uint8_t *data, uint16_t *size) override
    { if (!refresh()) { *size = 0; return false; } return inner->Read(data, size); }
    bool Write(const uint8_t *data, uint16_t *size) override
    {
        if ((flags & 0xf) != OPEN_WRITE && (flags & 0xf) != OPEN_READWRITE) {
            *size = 0; errno = EACCES; return error();
        }
        if (!storage->writable()) { *size = 0; return error(); }
        const bool ok = inner->Write(data, size);
        date = inner->date; time = inner->time; return ok;
    }
    bool Seek(uint32_t *pos, uint32_t type) override
    { return refresh() && inner->Seek(pos, type); }
    uint32_t GetSeekPos() override
    {
        uint32_t position = 0;
        return refresh() && inner->Seek(&position, DOS_SEEK_CUR) ? position : 0xffffffff;
    }
    uint16_t GetInformation() override { return inner->GetInformation(); }
    void Flush() override { if (storage->ready()) inner->Flush(); }
    bool FlushSucceeded() const override { return storage->ready() && inner->FlushSucceeded(); }
    bool UpdateDateTimeFromHost() override
    {
        if (!storage->ready()) return error();
        const bool ok = inner->UpdateDateTimeFromHost();
        date = inner->date; time = inner->time; return ok;
    }
    bool Close() override
    {
        if (refCtr > 1) return true;
        inner->date = date; inner->time = time; inner->newtime = newtime;
        const bool ok = inner->Close();
        delete inner; inner = nullptr; open = false; return ok;
    }
};

class UpperDrive final : public localDrive {
public:
    UpperDrive(const std::string &path, std::vector<std::string> &options)
        : localDrive((path + "/").c_str(), 512, 32, 0, 0, 0xf8, options)
    {}
    void detectAttributes(const std::string &path)
    {
        uint16_t attributes;
        native_dos_attributes = pidos_attributes::get(path.c_str(), attributes);
    }
};

class Drive final : public physfsDrive {
    std::shared_ptr<Overlay> storage;
    UpperDrive upper_drive;
    std::string archive, overlay, mountpoint;
    char letter;
    bool upper_mounted = false;
    struct Directory { std::string path; std::vector<std::string> entries; size_t next = 0; };

    bool resolve(const char *name, std::string &relative, bool expand = true)
    {
        std::string full = name;
        std::replace(full.begin(), full.end(), '\\', '/');
        const std::string base = "/" + mountpoint;
        if (full != base && full.compare(0, base.size() + 1, base + "/")) {
            while (!full.empty() && full[0] == '/') full.erase(0, 1);
            full = base + "/" + full;
        }
        if (full.size() >= CROSS_LEN) { errno = ENAMETOOLONG; return false; }
        char buffer[CROSS_LEN];
        strcpy(buffer, full.c_str()); CROSS_FILENAME(buffer);
        if (expand) dirCache.ExpandName(buffer);
        normalize(buffer, basedir);
        full = buffer;
        while (!full.empty() && full[0] == '/') full.erase(0, 1);
        if (full != mountpoint && full.compare(0, mountpoint.size() + 1, mountpoint + "/")) {
            errno = EACCES; return false;
        }
        relative = full.substr(mountpoint.size());
        if (!relative.empty() && relative[0] == '/') relative.erase(0, 1);
        if (!Overlay::valid(relative)) { errno = EACCES; return false; }
        return true;
    }
    void changed() { dirCache.EmptyCache(); upper_drive.EmptyCache(); }
    bool wrap(DOS_File **file, const char *name, const std::string &path, bool archived, bool ok)
    { if (ok) *file = new File(storage, *file, name, &upper_drive, path, archived); return ok; }
    bool exists(const std::string &path, bool directory)
    {
        PHYSFS_Stat st;
        return storage->stat(path, st) &&
               (st.filetype == PHYSFS_FILETYPE_DIRECTORY) == directory;
    }
public:
    Drive(char drive, const std::string &zip, const std::string &directory,
          bool read_only, int &failure, std::vector<std::string> &options)
        : physfsDrive(drive, (":" + zip + ":/").c_str(), 512, 32, 32765, 0, 0xf8,
                      failure, options),
          storage(new Overlay(zip, directory, std::string(1, drive) + "_DRIVE", read_only)),
          upper_drive(directory, options),
          archive(zip), overlay(directory), mountpoint(std::string(1, drive) + "_DRIVE"),
          letter(drive)
    {
        readonly = read_only;
        upper_drive.readonly = read_only;
        if (failure) return;
        if (!storage->init()) { failure = 11; return; }
        upper_drive.detectAttributes(directory);
        struct stat st;
        if (!::stat(directory.c_str(), &st)) {
            upper_mounted = PHYSFS_mount(directory.c_str(), mountpoint.c_str(), 0) != 0;
            if (!upper_mounted) { failure = 12; return; }
        }
        changed(); SetLabel("ZIP", false, false);
    }
    ~Drive() override
    {
        if (upper_mounted) PHYSFS_unmount(overlay.c_str());
        PHYSFS_unmount(archive.c_str());
    }
    const char *GetInfo() override
    {
        snprintf(info, sizeof(info), "ZIP %.180s (%s)", archive.c_str(), readonly ? "read-only" : ".PFS overlay");
        return info;
    }
    bool GetSystemFilename(char *, const char *) override { return false; }
    FILE *GetSystemFilePtr(const char *, const char *) override { return nullptr; }
    bool FileOpen(DOS_File **file, const char *name, uint32_t flags) override
    {
        std::string path;
        if (!resolve(name, path) || !exists(path, false)) return error();
        const bool writing = (flags & 0xf) != OPEN_READ && (flags & 0xf) != OPEN_READ_NO_MOD;
        if (writing && !storage->copyUp(path)) return error();
        if (storage->upper(path))
            return wrap(file, name, path, false, upper_drive.FileOpen(file, path.c_str(), flags));
        return wrap(file, name, path, true, physfsDrive::FileOpen(file, name, flags));
    }
    bool FileCreate(DOS_File **file, const char *name, uint16_t attributes) override
    {
        std::string path;
        if (!resolve(name, path) || !storage->prepareCreate(path)) return error();
        if (exists(path, true)) { errno = EACCES; return error(); }
        const bool ok = upper_drive.FileCreate(file, path.c_str(), attributes);
        changed(); return wrap(file, name, path, false, ok);
    }
    bool FileUnlink(const char *name) override
    {
        std::string path;
        uint16_t attr;
        if (!GetFileAttr(name, &attr)) return false;
        if (attr & DOS_ATTR_READ_ONLY) { errno = EACCES; return error(); }
        if (!resolve(name, path) || !storage->remove(path, false)) return error();
        changed(); return true;
    }
    bool MakeDir(const char *name) override
    {
        std::string path;
        PHYSFS_Stat st;
        if (!resolve(name, path) || !storage->prepareCreate(path)) return error();
        if (storage->stat(path, st)) { errno = EEXIST; return error(); }
        if (::mkdir(storage->host(path).c_str(), 0777)) return error();
        changed(); return true;
    }
    bool RemoveDir(const char *name) override
    {
        std::string path;
        if (!resolve(name, path) || !storage->remove(path, true)) return error();
        changed(); return true;
    }
    bool Rename(const char *from, const char *to) override
    {
        std::string source, target;
        uint16_t attributes;
        if (!GetFileAttr(from, &attributes)) return false;
        if (attributes & DOS_ATTR_READ_ONLY) { errno = EACCES; return error(); }
        if (!resolve(from, source) || !resolve(to, target) || !storage->rename(source, target)) return error();
        changed(); return true;
    }
    bool TestDir(const char *name) override
    { std::string path; return resolve(name, path) && exists(path, true); }
    bool FileExists(const char *name) override
    { std::string path; return resolve(name, path) && exists(path, false); }
    bool FileStat(const char *name, FileStat_Block *st) override
    {
        std::string path;
        if (!resolve(name, path) || !exists(path, false)) return error();
        return physfsDrive::FileStat(name, st);
    }
    bool FindNext(DOS_DTA &dta) override
    {
        while (physfsDrive::FindNext(dta)) {
            char name[DOS_NAMELENGTH_ASCII], longname[LFN_NAMELENGTH + 1];
            uint32_t size, high; uint16_t date, time; uint8_t attr;
            dta.GetResult(name, longname, size, high, date, time, attr);
            if (attr & DOS_ATTR_VOLUME) return true;
            const uint16_t id = lfn_filefind_handle >= LFN_FILEFIND_MAX ? dta.GetDirID() : ldid[lfn_filefind_handle];
            const std::string path = (lfn_filefind_handle >= LFN_FILEFIND_MAX ?
                                      srchInfo[id].srch_dir : ldir[lfn_filefind_handle]) + std::string(longname);
            uint16_t attributes;
            if (!GetFileAttr(path.c_str(), &attributes)) continue;
            char pattern[LFN_NAMELENGTH + 1]; uint8_t search;
            dta.GetSearchParams(search, pattern, false);
            if (attributes & ~search & (DOS_ATTR_HIDDEN | DOS_ATTR_SYSTEM | DOS_ATTR_DIRECTORY)) continue;
            dta.SetResult(name, longname, size, high, date, time, uint8_t(attributes));
            return true;
        }
        return false;
    }
    bool GetFileAttr(const char *name, uint16_t *attributes) override
    {
        std::string path;
        PHYSFS_Stat st;
        if (!resolve(name, path) || !storage->stat(path, st)) return error();
        if (storage->upper(path)) return upper_drive.GetFileAttr(path.c_str(), attributes);
        *attributes = st.filetype == PHYSFS_FILETYPE_DIRECTORY ? DOS_ATTR_DIRECTORY : DOS_ATTR_ARCHIVE;
        return true;
    }
    bool SetFileAttr(const char *name, uint16_t attributes) override
    {
        std::string path;
        if (!resolve(name, path) || !storage->copyUp(path)) return error();
        const bool ok = upper_drive.SetFileAttr(path.c_str(), attributes);
        changed(); return ok;
    }
    bool GetFileAttrEx(char *name, struct stat *st) override
    {
        std::string path; PHYSFS_Stat data;
        if (!resolve(name, path) || !storage->stat(path, data)) return error();
        memset(st, 0, sizeof(*st));
        st->st_mode = data.filetype == PHYSFS_FILETYPE_DIRECTORY ? S_IFDIR | 0555 : S_IFREG | 0444;
        if (!readonly) st->st_mode |= 0222;
        st->st_size = data.filesize; st->st_mtime = data.modtime;
        return true;
    }
    bool AllocationInfo(uint16_t *b, uint8_t *s, uint16_t *t, uint16_t *f) override
    {
        if (!storage->ready()) return error();
        const bool ok = upper_drive.AllocationInfo(b, s, t, f);
        if (readonly) *f = 0;
        return ok;
    }
    bool AllocationInfo64(uint32_t *b, uint32_t *s, uint64_t *t, uint64_t *f) override
    {
        if (!storage->ready()) return error();
        const bool ok = upper_drive.AllocationInfo64(b, s, t, f);
        if (readonly) *f = 0;
        return ok;
    }
    void *opendir(const char *name) override
    {
        std::string path;
        if (!resolve(name, path, false) || !exists(path, true)) return nullptr;
        auto *dir = new Directory;
        dir->path = path;
        if (!path.empty()) { dir->entries.emplace_back("."); dir->entries.emplace_back(".."); }
        const auto entries = storage->list(path);
        dir->entries.insert(dir->entries.end(), entries.begin(), entries.end());
        return dir;
    }
    void closedir(void *handle) override { delete static_cast<Directory *>(handle); }
    bool read_directory_first(void *handle, char *name, char *shortname, bool &directory) override
    { return read_directory_next(handle, name, shortname, directory); }
    bool read_directory_next(void *handle, char *name, char *shortname, bool &directory) override
    {
        auto *dir = static_cast<Directory *>(handle);
        if (!dir || !storage->ready()) return false;
        while (dir->next < dir->entries.size()) {
            const std::string entry = dir->entries[dir->next++];
            PHYSFS_Stat st;
            if (entry == "." || entry == "..") directory = true;
            else {
                if (!storage->stat(dir->path.empty() ? entry : dir->path + "/" + entry, st)) continue;
                directory = st.filetype == PHYSFS_FILETYPE_DIRECTORY;
            }
            safe_strncpy(name, entry.c_str(), CROSS_LEN); *shortname = 0; return true;
        }
        return false;
    }
    Bits UnMount() override
    {
        for (Bitu i = 0; i < DOS_FILES; ++i)
            if (Files[i] && Files[i]->IsOpen() && Files[i]->GetDrive() == letter - 'A') return 1;
        delete this; return 0;
    }
};
} // namespace pidos_zip

DOS_Drive *PI_DOS_MountZip(char letter, const char *filename, bool readonly, std::string &message)
{
    char resolved[PATH_MAX];
    if (!realpath(filename, resolved)) { message = "ZIP file not found."; return nullptr; }
    const std::string archive = resolved;
    if (archive.size() < 4 || strcasecmp(archive.c_str() + archive.size() - 4, ".ZIP") ||
        archive.find(':') != std::string::npos || archive.size() + 32 >= CROSS_LEN) {
        message = "Expected a ZIP file with a supported path length."; return nullptr;
    }
    if (physfs_used && PHYSFS_getMountPoint(archive.c_str())) {
        message = "This archive is already mounted."; return nullptr;
    }
    unsigned char signature[4] = {};
    FILE *in = fopen(archive.c_str(), "rb");
    if (!in) { message = "Cannot read ZIP file."; return nullptr; }
    const size_t count = fread(signature, 1, sizeof(signature), in); fclose(in);
    if (count != 4 || signature[0] != 'P' || signature[1] != 'K' ||
        !((signature[2] == 3 && signature[3] == 4) || (signature[2] == 5 && signature[3] == 6))) {
        message = "Not a supported ZIP archive."; return nullptr;
    }
    std::string overlay = archive.substr(0, archive.size() - 4) + ".PFS";
    // Reuse existing sibling spelling on case-sensitive host filesystems too.
    const size_t slash = overlay.rfind('/');
    DIR *parent = ::opendir(overlay.substr(0, slash).c_str());
    if (parent) {
        while (struct dirent *entry = ::readdir(parent))
            if (!strcasecmp(entry->d_name, overlay.c_str() + slash + 1)) {
                overlay = overlay.substr(0, slash + 1) + entry->d_name; break;
            }
        ::closedir(parent);
    }
    int failure = 0;
    std::vector<std::string> options;
    auto *drive = new pidos_zip::Drive(letter, archive, overlay, readonly, failure, options);
    if (failure) {
        delete drive;
        message = "Cannot mount ZIP or its .PFS folder (invalid archive, metadata or storage permissions).";
        return nullptr;
    }
    return drive;
}
#endif
#endif
