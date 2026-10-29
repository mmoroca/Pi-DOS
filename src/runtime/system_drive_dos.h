/* Included by DOSBox-X dos_programs.cpp. GPL-2.0-or-later. */
#ifndef PI_DOS_SYSTEM_DRIVE_DOS_H
#define PI_DOS_SYSTEM_DRIVE_DOS_H
#include "pidos_drive_policy.h"
#if defined(LINUX) && !defined(OSFREE)
#include <sys/stat.h>
namespace pidos_system {
class Drive final : public localDrive {
public:
    Drive(bool read_only, std::vector<std::string> &options)
        : localDrive("/dosdata/", 512, 32, 0, 0, 0xf8, options)
    {
        readonly = read_only;
        SetLabel("SYSTEM", false, false);
    }
    Bits UnMount() override { return 1; }
    bool SetLabelChecked(const char *) override
    { DOS_SetError(DOSERR_WRITE_PROTECTED); return false; }
};

class Install final : public Program {
public:
    void Run() override
    {
        if (!PiDosApplianceDrives() || Drives[2]) return;
        const char *storage = getenv("PI_DOS_SYSTEM_STORAGE");
        if (!storage || (strcmp(storage, "rw") && strcmp(storage, "ro"))) return;
        struct stat status;
        if (stat("/dosdata", &status) || !S_ISDIR(status.st_mode)) return;
        std::vector<std::string> options;
        Drives[2] = new Drive(!strcmp(storage, "ro"), options);
        mem_writeb(Real2Phys(dos.tables.mediaid) + 2 * 2, 0xf8);
        LOG_MSG("Pi-DOS: system C: registered (%s)", storage);
    }
};
static void program(Program **make) { *make = new Install; }
} // namespace pidos_system
#endif
#endif
