/* Internal appliance startup command, included by dos_programs.cpp. */
#ifndef PI_DOS_USB_CDROM_DOS_H
#define PI_DOS_USB_CDROM_DOS_H
#if defined(LINUX) && !defined(OSFREE)
namespace pidos_cdrom {
class Install final : public Program {
public:
    void Run() override
    {
        const char *enabled = getenv("PI_DOS_USB_CDROM");
        if (!enabled || strcmp(enabled, "1") || Drives[3]) return;
        int error = 0;
        std::vector<std::string> options;
        auto drive = new isoDrive('D', "pidos-usb-cdrom", 0xf8, error, options);
        if (error) { delete drive; LOG_MSG("Pi-DOS: USB CD-ROM D: registration failed (%d)", error); return; }
        DriveManager::AppendDisk(3, drive);
        DriveManager::InitializeDrive(3);
        mem_writeb(Real2Phys(dos.tables.mediaid) + 3 * 2, 0xf8);
        LOG_MSG("Pi-DOS: USB CD-ROM D: automatic discovery enabled (digital audio)");
    }
};
static void program(Program **make) { *make = new Install; }
} // namespace pidos_cdrom
#endif
#endif
