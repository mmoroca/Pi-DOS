/* Fake sysfs and MMC device; exercises production backend and prefetch worker. */
#include <algorithm>
#include <array>
#include <atomic>
#include <cassert>
#include <cerrno>
#include <chrono>
#include <climits>
#include <condition_variable>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <fcntl.h>
#include <functional>
#include <glob.h>
#include <linux/cdrom.h>
#include <linux/fs.h>
#include <mutex>
#include <scsi/sg.h>
#include <string>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <thread>
#include <unistd.h>
#include <vector>

using Bitu = unsigned long;
using PhysPt = uint32_t;
#include "test-cdrom-abi.h"

static std::atomic<bool> inserted{true}, attention{false}, changed{false}, audio_only{false};
static std::atomic<bool> short_read{false}, broken_audio{false}, disconnected{false};
static std::atomic<bool> read_attention{false};
static std::atomic<unsigned> audio_reads{0}, opens{0}, closes{0};
static std::atomic<uint8_t> revision{0x31};
static bool guest_if = true;
static void (*audio_callback)(Bitu) = nullptr;
static std::vector<int16_t> mixed;
static std::vector<uint8_t> memory(1 << 20);

class MixerChannel {
public:
    bool enabled = false;
    void Enable(bool value) { enabled = value; }
    void AddSamples_s16(Bitu frames, const int16_t *data) { mixed.assign(data, data + frames * 2); }
};
static MixerChannel *MIXER_AddChannel(void (*callback)(Bitu), Bitu rate, const char *name)
{ assert(rate == 44100 && !strcmp(name, "USBCD")); audio_callback = callback; return new MixerChannel; }
static void MIXER_DelChannel(MixerChannel *channel) { delete channel; audio_callback = nullptr; }
static void MEM_BlockWrite(PhysPt address, const void *data, size_t bytes)
{ assert(address + bytes <= memory.size()); memcpy(memory.data() + address, data, bytes); }
#define GETFLAG(flag) guest_if
#define SETFLAGBIT(flag, value) (guest_if = (value))
void CALLBACK_IdleNoInts() { assert(!guest_if); std::this_thread::yield(); }

static int fake_open(const char *path, int flags)
{
    assert(strstr(path, "/sr0"));
    assert((flags & O_ACCMODE) == O_RDONLY && (flags & O_NONBLOCK));
    if (disconnected) { errno = ENODEV; return -1; }
    ++opens; return 901;
}
static int fake_close(int fd) { assert(fd == 901); ++closes; return 0; }
static int fake_fstat(int fd, struct stat *status)
{ assert(fd == 901); memset(status, 0, sizeof(*status)); status->st_mode = S_IFBLK; return 0; }
static int fake_ioctl(int fd, unsigned long command, int)
{
    assert(fd == 901);
    if (disconnected) { errno = ENODEV; return -1; }
    if (command == CDROM_MEDIA_CHANGED) return changed.exchange(false) ? 1 : 0;
    if (command == CDROM_DRIVE_STATUS) return inserted ? CDS_DISC_OK : CDS_TRAY_OPEN;
    if (command == CDROMEJECT || command == CDROMCLOSETRAY) {
        inserted = command == CDROMCLOSETRAY; changed = true; return 0;
    }
    assert(false); return -1;
}
static void put32(uint8_t *out, uint32_t value)
{ out[0] = value >> 24; out[1] = value >> 16; out[2] = value >> 8; out[3] = value; }
static int fake_ioctl(int fd, unsigned long command, void *argument)
{
    assert(fd == 901);
    if (command == CDROM_GET_MCN) {
        auto *mcn = static_cast<cdrom_mcn *>(argument);
        memcpy(mcn->medium_catalog_number, "1234567890123", 14); return 0;
    }
    assert(command == SG_IO);
    auto *io = static_cast<sg_io_hdr_t *>(argument);
    auto *cdb = io->cmdp;
    assert(io->timeout == 3000 && io->interface_id == 'S');
    assert(io->dxfer_direction == SG_DXFER_FROM_DEV || io->dxfer_direction == SG_DXFER_NONE);
    if (disconnected) { io->host_status = 1; return 0; }
    if (attention.exchange(false) || ((cdb[0] == 0x28) && read_attention.exchange(false)) || !inserted) {
        io->status = 2; io->driver_status = 8; io->sb_len_wr = 18;
        io->sbp[0] = 0x70; io->sbp[2] = inserted ? 6 : 2;
        io->sbp[12] = inserted ? 0x28 : 0x3a; return 0;
    }
    if (cdb[0] == 0) { assert(!io->dxfer_len); return 0; }
    auto *data = static_cast<uint8_t *>(io->dxferp);
    if (cdb[0] == 0x43) {
        memset(data, 0, io->dxfer_len);
        const unsigned tracks = audio_only ? 1 : 2, size = 4 + (tracks + 1) * 8;
        data[1] = size - 2; data[2] = 1; data[3] = tracks;
        data[5] = audio_only ? 0x10 : 0x14; data[6] = 1; put32(data + 8, 0);
        if (!audio_only) { data[13] = 0x10; data[14] = 2; put32(data + 16, 40); }
        data[4 + tracks * 8 + 2] = 0xaa; put32(data + 4 + tracks * 8 + 4, 90);
        io->resid = io->dxfer_len - size; return 0; // normal short TOC response
    }
    assert(cdb[0] == 0x28 || cdb[0] == 0xbe);
    if (cdb[0] == 0xbe && cdb[1] == 4) {
        assert(cdb[9] == 0x10);
        ++audio_reads;
        if (broken_audio) { io->status = 2; io->sbp[0] = 0x70; io->sbp[2] = 5; return 0; }
        for (unsigned i = 0; i < io->dxfer_len; i += 4) {
            data[i] = 0xe8; data[i + 1] = 3; // left +1000
            data[i + 2] = 0x30; data[i + 3] = 0xf8; // right -2000
        }
    } else {
        memset(data, revision, io->dxfer_len);
    }
    if (short_read) io->resid = 1;
    return 0;
}

#define open fake_open
#define close fake_close
#define fstat fake_fstat
#define ioctl fake_ioctl
#define LINUX 1
#include "pidos_usb_cdrom_backend.h"
#undef open
#undef close
#undef fstat
#undef ioctl

// Minimal filesystem ABI around the actual production media-refresh and file
// read methods, extracted by the runner. Sector I/O still reaches the real USB
// backend above; only filesystem parsing and DOS error storage are stubbed.
enum { DOSERR_DRIVE_NOT_READY = 21, ISO_FRAMESIZE = 2048 };
static unsigned dos_error = 0;
static void DOS_SetError(unsigned error) { dos_error = error; }
struct isoDirEntry { uint8_t bytes[256]; };
#include "pidos_dos_sharing.h"
class isoDrive {
public:
    CDROM_Interface *cdrom;
    uint64_t usb_generation = 0;
    uint64_t sharing_media = pidos_sharing::next_media_id();
    bool usb_loading = false, usb_ready = false, dataCD = false, iso = false;
    bool is_joliet = false, is_udf = false, is_rock_ridge = false;
    char curdir[256] = {}, discLabel[32] = {};
    int nextFreeDirIterator = 0, subUnit = 0;
    struct DirIterator { bool valid; };
    std::array<DirIterator, 4> dirIterators = {};
    uint8_t sectorHashEntries[64] = {};
    isoDirEntry rootEntry = {};
    explicit isoDrive(CDROM_Interface *source) : cdrom(source) {}
    bool USBReady(uint64_t expected = 0);
    uint64_t USBGeneration() const;
    const char *GetLabel();
    bool readSector(uint8_t *buffer, uint32_t sector) const;
    bool loadImage() { std::array<uint8_t, 2048> data; dataCD = readSector(data.data(), 16); return dataCD; }
    size_t UDFextent_read(int &, uint8_t *, size_t) { assert(false); return 0; }
};
class isoFile {
public:
    isoDrive *drive;
    uint64_t usb_generation;
    bool udf = false;
    int udffext = 0;
    uint32_t filePos = 16 * 2048, fileEnd = 17 * 2048;
    int cachedSector = -1;
    uint8_t buffer[2048] = {};
    explicit isoFile(isoDrive *source) : drive(source), usb_generation(source->USBGeneration()) {}
    bool Read(uint8_t *, uint16_t *);
};
static bool CDROM_IsAudioOnly(CDROM_Interface *cd) { return cd->IsAudioOnly(); }
static bool MSCDEX_GetVolumeName(int, char *label) { sprintf(label, "DISC%02X", revision.load()); return true; }
static void Set_Label(const char *source, char *destination, bool) { strcpy(destination, source); }
#include "test-cdrom-media.h"

int main(int argc, char **argv)
{
    assert(argc == 2);
    const std::string fixture = argv[1], sys = fixture + "/sys/class/block";
    assert(pidos_cdrom::candidate(sys + "/sr0"));
    assert(!pidos_cdrom::candidate(sys + "/sda"));
    assert(!pidos_cdrom::candidate(sys + "/sr1"));
    // The production backend uses fixed appliance paths. Redirect those
    // constructor defaults in the copied test header, not the production API.
    {
        pidos_cdrom::Interface cd;
        bool present = false, change = false, tray = false;
        assert(cd.SetDevice("pidos-usb-cdrom", -1));
        cd.GetMediaTrayStatus(present, change, tray);
        assert(present && change && !tray);
        const uint64_t epoch = cd.MediaGeneration();
        cd.GetMediaTrayStatus(present, change, tray);
        assert(present && !change);
        int first, last; TMSF lead, begin; unsigned char attr;
        assert(cd.GetAudioTracks(first, last, lead) && first == 1 && last == 2);
        assert(lead.sec == 3 && lead.fr == 15); // LBA 90 + 150
        assert(cd.GetAudioTrackInfo(2, begin, attr) && !(attr & 0x40) && begin.sec == 2 && begin.fr == 40);
        assert(cd.ReadSectors(0, false, 16, 1) && memory[0] == 0x31);
        assert(cd.ReadSectors(0, true, 16, 1));
        short_read = true;
        std::fill(memory.begin(), memory.end(), 0);
        assert(!cd.ReadSectors(0, false, 16, 1) && memory[0] == 0);
        short_read = false;
        assert(!cd.ReadSectors(0, false, 89, 2));
        assert(cd.ReadSectors(0, false, 0, 0));
        assert(!cd.PlayAudioSector(0, 10)); // data tracks must never become noise
        assert(!cd.PlayAudioSector(80, 11)); // past lead-out
        assert(cd.PlayAudioSector(40, 16));
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
        while (audio_reads < 2 && std::chrono::steady_clock::now() < deadline) std::this_thread::yield();
        assert(audio_reads >= 2);
        bool playing, paused;
        assert(cd.GetAudioStatus(playing, paused) && playing && !paused); // synchronize prefetch completion
        audio_callback(588);
        assert(mixed[0] == 1000 && mixed[1] == -2000);
        unsigned char track, index; TMSF rel, abs;
        assert(cd.GetAudioSub(attr, track, index, rel, abs));
        assert(track == 2 && index == 1 && rel.fr == 1 && abs.fr == 41);
        assert(cd.PauseAudio(false));
        audio_callback(588); assert(mixed[0] == 0 && mixed[1] == 0);
        assert(cd.GetAudioSub(attr, track, index, rel, abs) && rel.fr == 1);
        assert(cd.PauseAudio(true));
        TCtrl control = {{1, 0, 2, 3}, {255, 128, 255, 255}};
        cd.ChannelControl(control); audio_callback(588);
        assert(mixed[0] == -2000 && mixed[1] == 501);
        assert(cd.StopAudio());
        assert(cd.GetAudioStatus(playing, paused) && !playing && !paused);
        isoDrive dos_drive(&cd);
        assert(dos_drive.USBReady() && !strcmp(dos_drive.GetLabel(), "DISC31"));
        const auto sharing_media = dos_drive.sharing_media;
        isoFile old_file(&dos_drive);
        uint8_t data[32]; uint16_t size = sizeof(data);
        assert(old_file.Read(data, &size) && size == sizeof(data) && data[0] == 0x31);
        strcpy(dos_drive.curdir, "OLD_DIR");
        // Equal-length disc swap: unit attention, not the lead-out, changes identity.
        revision = 0x52; attention = true;
        cd.GetMediaTrayStatus(present, change, tray);
        assert(present && change && cd.MediaGeneration() != epoch);
        size = sizeof(data);
        assert(!old_file.Read(data, &size) && size == 0 && dos_error == DOSERR_DRIVE_NOT_READY);
        assert(!*dos_drive.curdir && !strcmp(dos_drive.GetLabel(), "DISC52"));
        assert(dos_drive.sharing_media != sharing_media);
        isoFile new_file(&dos_drive); size = sizeof(data);
        assert(new_file.Read(data, &size) && data[0] == 0x52);
        isoFile interrupted_file(&dos_drive); size = sizeof(data); read_attention = true;
        assert(!interrupted_file.Read(data, &size)); // media change during sector I/O
        assert(cd.PlayAudioSector(40, 16));
        assert(cd.ReadSectors(0, false, 16, 1) && memory[0] == 0x52);
        assert(cd.LoadUnloadMedia(true));
        cd.GetMediaTrayStatus(present, change, tray);
        assert(!present && tray && !cd.ReadSectors(0, false, 16, 1));
        audio_callback(588); assert(mixed[0] == 0 && mixed[1] == 0);
        size = sizeof(data); assert(!new_file.Read(data, &size));
        assert(cd.LoadUnloadMedia(false));
        audio_only = true; changed = true;
        cd.GetMediaTrayStatus(present, change, tray);
        assert(present && cd.IsAudioOnly());
        assert(cd.GetAudioTracks(first, last, lead) && first == 1 && last == 1);
        char upc[14]; assert(cd.GetUPC(attr, upc) && !strcmp(upc, "1234567890123"));
        broken_audio = true;
        assert(cd.PlayAudioSector(0, 16));
        const auto timeout = std::chrono::steady_clock::now() + std::chrono::seconds(2);
        do { cd.GetAudioStatus(playing, paused); } while (playing && std::chrono::steady_clock::now() < timeout);
        assert(!playing); // unsupported READ CD fails cleanly, no drive-play fallback
        broken_audio = false;
        disconnected = true;
        cd.GetMediaTrayStatus(present, change, tray);
        assert(!present);
        disconnected = false;
        const auto reconnect = std::chrono::steady_clock::now() + std::chrono::seconds(2);
        do { cd.GetMediaTrayStatus(present, change, tray); } while (!present && std::chrono::steady_clock::now() < reconnect);
        assert(present && opens >= 2 && closes >= 1);
        assert(guest_if);
    }
    disconnected = true;
    {
        pidos_cdrom::Interface empty;
        assert(empty.SetDevice("pidos-usb-cdrom", -1));
        isoDrive dos_drive(&empty);
        assert(!dos_drive.USBReady() && !*dos_drive.GetLabel());
    }
    assert(opens == closes);
}
