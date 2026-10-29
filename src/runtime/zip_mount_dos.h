/* Included by dos_programs.cpp. GPL-2.0-or-later. */
#ifndef PI_DOS_ZIP_MOUNT_DOS_H
#define PI_DOS_ZIP_MOUNT_DOS_H
#if defined(LINUX) && !defined(OSFREE)
static const char *UnmountHelper(char drive);
DOS_Drive *PI_DOS_MountZip(char letter, const char *filename, bool readonly, std::string &message);
namespace pidos_zip_mount {
class Command final : public Program {
public:
    void Run() override
    {
        ChangeToLongCmd();
        dos.return_code = 1;
        if (cmd->FindExist("/?", true) || !cmd->GetCount()) {
            WriteOut_NoParsing("Mount a ZIP with a persistent sibling .PFS folder.\n"
                               "ZIPMOUNT drive archive.zip [-ro]\n"
                               "ZIPMOUNT drive -u\n"
                               "Archive paths use existing DOS drives. -ro disables writes.\n"
                               "Example: ZIPMOUNT E C:\\GAMES\\DOOM.ZIP\n");
            dos.return_code = 0;
            return;
        }
        if (control->SecureMode()) { WriteOut_NoParsing("ZIPMOUNT is unavailable in secure mode.\n"); return; }
        const bool readonly = cmd->FindExist("-ro", true);
        const bool unmount = cmd->FindExist("-u", true);
        std::string target, path;
        if (!cmd->FindCommand(1, target) || target.empty() || target.size() > 2 ||
            (target.size() == 2 && target[1] != ':')) {
            WriteOut_NoParsing("Specify a DOS drive letter.\n"); return;
        }
        const char letter = char(toupper(static_cast<unsigned char>(target[0])));
        if (letter < 'A' || letter > 'Y' || letter == 'C') {
            WriteOut_NoParsing("Choose an available drive other than C: or Z:.\n"); return;
        }
        const int drive = letter - 'A';
        if (unmount) {
            if (readonly || cmd->GetCount() != 1) {
                WriteOut_NoParsing("Usage: ZIPMOUNT drive -u\n"); return;
            }
            const bool existed = Drives[drive] != nullptr;
            const char *message = UnmountHelper(letter);
            if (existed && !Drives[drive]) dos.return_code = 0;
            WriteOut(message, letter); return;
        }
        if (Drives[drive] || (drive < MAX_DISK_IMAGES && imageDiskList[drive])) {
            WriteOut_NoParsing("Drive is occupied. Unmount it first or choose a free letter.\n"); return;
        }
        if (cmd->GetCount() != 2 || !cmd->FindCommand(2, path)) {
            WriteOut_NoParsing("Usage: ZIPMOUNT drive archive.zip [-ro]\n"); return;
        }
        char dosname[DOS_PATHLENGTH], host[CROSS_LEN];
        uint8_t source;
        if (!DOS_MakeName(path.c_str(), dosname, &source)) {
            WriteOut_NoParsing("ZIP path must name a file on an available DOS drive.\n"); return;
        }
        auto *local = dynamic_cast<localDrive *>(Drives[source]);
        uint16_t attributes;
        if (!local || dynamic_cast<physfsDrive *>(local) ||
            !Drives[source]->GetFileAttr(dosname, &attributes) ||
            (attributes & DOS_ATTR_DIRECTORY) || !local->GetSystemFilename(host, dosname)) {
            WriteOut_NoParsing("ZIP files must be on C: or a host-backed DOS drive.\n"); return;
        }
        std::string message;
        const bool read_only = readonly || Drives[source]->readonly;
        DOS_Drive *mounted = PI_DOS_MountZip(letter, host, read_only, message);
        if (!mounted) { WriteOut("ZIPMOUNT: %s\n", message.c_str()); return; }
        Drives[drive] = mounted;
        mem_writeb(Real2Phys(dos.tables.mediaid) + drive * 2, 0xf8);
        Drives[source]->EmptyCache();
        dos.return_code = 0;
        WriteOut("ZIP mounted as %c: (%s).\n", letter, read_only ? "read-only" : "persistent .PFS overlay");
    }
};
static void program(Program **make) { *make = new Command; }
} // namespace pidos_zip_mount
#endif
#endif
