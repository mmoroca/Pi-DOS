/* Exercise real PhysFS ZIP reads and production overlay storage. */
#include "physfs.h"
#include "zip_overlay.h"
#include <cassert>
#include <fstream>
#include <iostream>

using pidos_zip::Overlay;
#define CROSS_LEN 4096
static void strreplace_dbcs(char *value, char from, char to)
{ for (; *value; ++value) if (*value == from) *value = to; }
#define CROSS_FILENAME(value) strreplace_dbcs(value, '\\', '/')
#include "zip-path-method.h"
static std::string read(const std::string &path)
{
    PHYSFS_File *file = PHYSFS_openRead(path.c_str());
    assert(file);
    std::string result(size_t(PHYSFS_fileLength(file)), '\0');
    assert(PHYSFS_readBytes(file, &result[0], result.size()) == PHYSFS_sint64(result.size()));
    assert(PHYSFS_close(file));
    return result;
}
static void write(const std::string &path, const std::string &data)
{ std::ofstream out(path, std::ios::binary | std::ios::trunc); out << data; out.close(); assert(out); }
static bool contains(const std::vector<std::string> &entries, const std::string &name)
{ return std::find(entries.begin(), entries.end(), name) != entries.end(); }
int main(int argc, char **argv)
{
    assert(argc == 2);
    const std::string root = argv[1], zip = root + "/DOOM.ZIP", upper = root + "/DOOM.PFS";
    PathProbe paths; std::string path;
    assert(paths.resolve("", path) && path.empty());
    assert(paths.resolve("DEFAULT.CFG", path) && path == "DEFAULT.CFG");
    assert(paths.resolve("\\DATA\\FILE.DAT", path) && path == "DATA/FILE.DAT");
    assert(paths.resolve("/E_DRIVE/DATA/", path, false) && path == "DATA");
    assert(paths.resolve("\\E_DRIVE\\DATA\\FILE.DAT", path, false) && path == "DATA/FILE.DAT");
    assert(!paths.resolve("../ESCAPE", path));
    assert(!paths.resolve("PFSINFO/DELETE.DAT", path));
    assert(PHYSFS_init(argv[0]));
    assert(PHYSFS_mount(zip.c_str(), "E_DRIVE", 1));
    Overlay store(zip, upper, "E_DRIVE", false);
    assert(store.init());
    assert(PHYSFS_mount(upper.c_str(), "E_DRIVE", 0));
    assert(read("E_DRIVE/DEFAULT.CFG") == "original config");
    assert(!store.upper("DEFAULT.CFG"));
    assert(store.copyUp("DEFAULT.CFG"));
    write(store.host("DEFAULT.CFG"), "new"); // overwrite and truncate, no append mode
    assert(read("E_DRIVE/DEFAULT.CFG") == "new");
    assert(!store.upper("DOOM.EXE"));
    assert(store.prepareCreate("DATA/SAVE.SAV"));
    write(store.host("DATA/SAVE.SAV"), "save data");
    assert(read("E_DRIVE/DATA/SAVE.SAV") == "save data");
    assert(store.remove("DATA/ORIG.DAT", false));
    PHYSFS_Stat st;
    assert(!store.stat("DATA/ORIG.DAT", st));
    assert(!contains(store.list("DATA"), "ORIG.DAT"));
    assert(!contains(store.list(""), "PFSINFO"));
    assert(!store.remove("DATA", true)); // nonempty merged directory
    assert(store.rename("DOOM.EXE", "RENAMED.EXE"));
    assert(!store.stat("DOOM.EXE", st));
    assert(read("E_DRIVE/RENAMED.EXE") == "executable");
    assert(store.rename("DATA", "MOVED"));
    assert(!store.stat("DATA", st));
    assert(!store.stat("MOVED/ORIG.DAT", st));
    assert(read("E_DRIVE/MOVED/SAVE.SAV") == "save data");
    assert(store.prepareCreate("DATA"));
    assert(!mkdir(store.host("DATA").c_str(), 0777));
    assert(store.list("DATA").empty()); // recreating a directory does not revive lower children
    assert(store.remove("EMPTY", true));
    assert(!store.stat("EMPTY", st));
    assert(!store.prepareCreate("../ESCAPE"));
    assert(!store.prepareCreate("PFSINFO/DELETE.DAT"));
    assert(!symlink(root.c_str(), store.host("LINK").c_str()));
    assert(!store.prepareCreate("LINK/ESCAPE"));

    // Independent mounts never share a global PhysFS write directory.
    const std::string second = root + "/OTHER.ZIP", other = root + "/OTHER.PFS";
    assert(PHYSFS_mount(second.c_str(), "F_DRIVE", 1));
    Overlay two(second, other, "F_DRIVE", false);
    assert(two.init()); assert(PHYSFS_mount(other.c_str(), "F_DRIVE", 0));
    assert(two.copyUp("DEFAULT.CFG"));
    write(two.host("DEFAULT.CFG"), "other config");
    assert(read("F_DRIVE/DEFAULT.CFG") == "other config");
    assert(read("E_DRIVE/DEFAULT.CFG") == "new");
    assert(PHYSFS_getWriteDir() == nullptr);
    assert(PHYSFS_unmount(other.c_str())); assert(PHYSFS_unmount(second.c_str()));
    Overlay no_overlay(second, root + "/MISSING.PFS", "F_DRIVE", true);
    assert(no_overlay.init());
    struct stat absent;
    assert(::lstat((root + "/MISSING.PFS").c_str(), &absent) && errno == ENOENT);
    write(root + "/BAD.PFS", "not a directory");
    Overlay bad(second, root + "/BAD.PFS", "F_DRIVE", false);
    assert(!bad.init());

    assert(PHYSFS_unmount(upper.c_str())); assert(PHYSFS_unmount(zip.c_str()));
    assert(PHYSFS_mount(zip.c_str(), "G_DRIVE", 1));
    Overlay remount(zip, upper, "G_DRIVE", false);
    assert(remount.init()); assert(PHYSFS_mount(upper.c_str(), "G_DRIVE", 0));
    assert(!remount.stat("DOOM.EXE", st));
    assert(remount.hidden("doom.exe"));
    assert(!remount.stat("EMPTY", st));
    assert(remount.list("DATA").empty());
    assert(read("G_DRIVE/DEFAULT.CFG") == "new");
    Overlay readonly(zip, upper, "G_DRIVE", true);
    assert(readonly.init());
    assert(!readonly.copyUp("DOOM.EXE"));
    assert(!readonly.prepareCreate("NEW.TXT"));
    assert(!readonly.remove("DEFAULT.CFG", false));
    assert(!readonly.rename("DEFAULT.CFG", "NEW.CFG"));
    assert(readonly.stat("DEFAULT.CFG", st));
    assert(!::rename(zip.c_str(), (zip + ".OLD").c_str()));
    assert(!remount.ready()); assert(!remount.prepareCreate("WRONG.SAV"));
    assert(!::rename((zip + ".OLD").c_str(), zip.c_str()));
    assert(PHYSFS_unmount(upper.c_str())); assert(PHYSFS_unmount(zip.c_str()));
    assert(PHYSFS_deinit());
    std::cout << "ZIP overlay: copy-on-write, saves, deletion, rename, remount, isolation and read-only checks passed\n";
}
