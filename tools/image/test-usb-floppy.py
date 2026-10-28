#!/usr/bin/env python3
"""USB floppy discovery/media safety with fake sysfs and SCSI; no root or hardware."""
from pathlib import Path
import os
import re
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]

with tempfile.TemporaryDirectory(prefix='pi-dos-floppy.') as temporary:
    work = Path(temporary)
    block = work / 'sys/class/block'
    interface = work / 'sys/devices/usb/1-1/1-1:1.0'
    scsi = interface / 'host0/target0:0:0/0:0:0:0'
    scsi.mkdir(parents=True)
    block.mkdir(parents=True)
    disk = block / 'sda'
    disk.mkdir()
    (disk / 'device').symlink_to(scsi)
    (disk / 'removable').write_text('1\n')
    (disk / 'queue').mkdir()
    (disk / 'queue/max_sectors_kb').write_text('32\n')
    (scsi / 'type').write_text('0\n')
    (scsi / 'model').write_text('USB FDD\n')
    (interface / 'bInterfaceClass').write_text('08\n')
    (interface / 'bInterfaceSubClass').write_text('04\n')
    (interface / 'bInterfaceProtocol').write_text('00\n')
    for name, value in {'idVendor': '0644', 'idProduct': '0000', 'bcdDevice': '0000', 'speed': '12'}.items():
        (interface.parent / name).write_text(value + '\n')
    endpoint = interface / 'ep_83'
    endpoint.mkdir()
    for name, value in {'bEndpointAddress': '83', 'bmAttributes': '03', 'wMaxPacketSize': '0002',
                        'bInterval': 'ff', 'interval': '255ms', 'type': 'Interrupt', 'direction': 'in'}.items():
        (endpoint / name).write_text(value + '\n')
    source = work / 'check.cpp'
    runtime = next((ROOT / 'output/buildroot-arm64/build').glob('pi-dos-runtime-*'))
    dispatcher = (runtime / 'src/dos/dos.cpp').read_text()
    if 'PI_DOS_USBFloppyDPB(Drives[drive], drive)' not in dispatcher:
        dos_source = work / 'src/dos/dos.cpp'
        dos_source.parent.mkdir(parents=True)
        dos_source.write_text(dispatcher)
        subprocess.run(['patch', '-d', str(work), '-p1', '--batch', '--fuzz=0', '-i',
                        str(ROOT / 'patches/dosbox-x/0018-usb-floppy-dpb.patch')],
                       check=True, capture_output=True, text=True)
        dispatcher = dos_source.read_text()
    start = dispatcher.index('        case 0x32: /* Get drive parameter block')
    handler = dispatcher[start:dispatcher.index('        case 0x33:', start)]
    (work / 'dpb-handler.h').write_text('''
enum { DOS_DRIVES = 26 };
static uint8_t default_drive = 0;
static uint8_t DOS_GetDefaultDrive() { return default_drive; }
static void SegSet16(uint16_t &segment, uint16_t value) { segment = value; }
static PhysPt SegPhys(uint16_t segment) { return uint32_t(segment) << 4; }
static bool DOS_GetFreeDiskSpace32(uint8_t, uint32_t *, uint32_t *, uint32_t *, uint32_t *) { return false; }
static void fake_log(const char *, ...) {}
#define LOG(...) fake_log
static void dpb_handler() {
    switch (reg_ah) {
    case 0x1f: goto case_0x32_fallthrough;
''' + handler + '''
    }
}
#undef LOG
''')
    source.write_text(r'''
#include <cassert>
#include <algorithm>
#include <cerrno>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <fcntl.h>
#include <linux/fs.h>
#include <scsi/sg.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/sysmacros.h>
#include <unistd.h>
#include <thread>
#include <chrono>
static bool claimed = false, tab = false, rdonly_fd = false, absent = false, short_read = false;
static bool mode_failure = false;
static unsigned int latency_ms = 0;
static bool reject_cross_track = false, reject_errno = false, change_on_reject = false;
static unsigned int command_limit = 64, rejected = 0, largest_read = 0, largest_write = 0;
static unsigned int attention = 0, writes = 0, commands = 0, reads = 0, capacity = 2880;
static uint64_t sequence = 10;
static unsigned char boot[512];
static bool persistent = false;
static unsigned char media[5760 * 512];
static int fake_open(const char *, int flags) {
    if (claimed) { errno = EBUSY; return -1; }
    if (rdonly_fd && (flags & O_RDWR)) { errno = EROFS; return -1; }
    assert(flags & O_EXCL); claimed = true; return 1234;
}
static int fake_close(int fd) { assert(fd == 1234 && claimed); claimed = false; return 0; }
static int fake_fstat(int, struct stat *st) {
    memset(st, 0, sizeof(*st)); st->st_mode = S_IFBLK; st->st_rdev = makedev(240, 123); return 0;
}
static void sense(sg_io_hdr_t *io, int key, int asc) {
    io->status = 2; io->info = SG_INFO_CHECK; io->sb_len_wr = 18;
    io->sbp[0] = 0x70; io->sbp[2] = key; io->sbp[12] = asc;
}
static int fake_ioctl(int fd, unsigned long request, void *arg) {
    assert(fd == 1234 && claimed);
    if (request == BLKGETDISKSEQ) { *static_cast<uint64_t *>(arg) = sequence; return 0; }
    assert(request == SG_IO);
    if (latency_ms) std::this_thread::sleep_for(std::chrono::milliseconds(latency_ms));
    auto io = static_cast<sg_io_hdr_t *>(arg);
    io->duration = latency_ms;
    assert(io->cmd_len == 12 && io->timeout == 3000); ++commands;
    if (attention) { --attention; sense(io, 6, 0x28); return 0; }
    if (absent) { sense(io, 2, 0x3a); return 0; }
    if (io->cmdp[0] == 0x28 || io->cmdp[0] == 0x2a) {
        const uint32_t first = (uint32_t(io->cmdp[2]) << 24) | (uint32_t(io->cmdp[3]) << 16) |
                               (uint32_t(io->cmdp[4]) << 8) | io->cmdp[5];
        const unsigned int count = io->dxfer_len / 512;
        if (count > command_limit || (reject_cross_track && first / 18 != (first + count - 1) / 18)) {
            ++rejected;
            if (change_on_reject) attention = 1;
            if (reject_errno) { errno = EINVAL; return -1; }
            sense(io, 5, 0x24); return 0;
        }
        if (io->cmdp[0] == 0x28) largest_read = std::max(largest_read, count);
        else largest_write = std::max(largest_write, count);
    }
    auto data = static_cast<unsigned char *>(io->dxferp);
    switch (io->cmdp[0]) {
    case 0: break;
    case 0x25:
        data[0] = (capacity - 1) >> 24; data[1] = (capacity - 1) >> 16;
        data[2] = (capacity - 1) >> 8; data[3] = capacity - 1;
        data[6] = 2; break;
    case 0x28: {
        assert(io->dxfer_len % 512 == 0); ++reads;
        const uint32_t sector = (uint32_t(io->cmdp[2]) << 24) | (uint32_t(io->cmdp[3]) << 16) |
                                (uint32_t(io->cmdp[4]) << 8) | io->cmdp[5];
        if (persistent) memcpy(data, media + sector * 512, io->dxfer_len);
        else { memset(data, 0, io->dxfer_len); memcpy(data, boot, 512); }
        if (short_read) io->resid = 1;
        break; }
    case 0x2a: {
        if (tab) sense(io, 7, 0x27);
        else {
            ++writes;
            const uint32_t sector = (uint32_t(io->cmdp[2]) << 24) | (uint32_t(io->cmdp[3]) << 16) |
                                    (uint32_t(io->cmdp[4]) << 8) | io->cmdp[5];
            if (persistent) memcpy(media + sector * 512, data, io->dxfer_len);
        }
        break; }
    case 0x5a:
        if (mode_failure) sense(io, 5, 0x20); else data[3] = tab ? 0x80 : 0;
        break;
    default: assert(false);
    }
    return 0;
}
#define open fake_open
#define close fake_close
#define fstat fake_fstat
#define ioctl fake_ioctl
#include "usb_floppy_linux.h"
#undef open
#undef close
#undef fstat
#undef ioctl
#include "usb-floppy-test-dos.h"
#include "pidos_usb_floppy_dos.h"
#include "dpb-handler.h"
using namespace pidos_floppy;
static void le16(int at, unsigned int value) { boot[at] = value; boot[at + 1] = value >> 8; }
class CopyFile : public DOS_File {
    std::shared_ptr<Device> device;
    unsigned int count;
public:
    explicit CopyFile(std::shared_ptr<Device> source, unsigned int sectors = 18) : device(source), count(sectors) {}
    bool Write(const uint8_t *, uint16_t *) override {
        unsigned char sector[512];
        for (unsigned int i = 0; i < count; ++i) {
            memset(sector, i + 1, sizeof(sector));
            assert(device->sector(36 + i, sector, true) == Result::ok);
            assert(device->sector(1, sector, true) == Result::ok);
            assert(device->sector(10, sector, true) == Result::ok);
        }
        assert(device->sector(19, sector, true) == Result::ok);
        memset(sector, 0, sizeof(sector));
        assert(device->sector(1, sector) == Result::ok && sector[0] == count);
        return true;
    }
};
int main(int argc, char **argv) {
    assert(argc == 3);
    le16(11, 512); boot[13] = 1; le16(14, 1); boot[16] = 2;
    le16(17, 224); le16(19, 2880); le16(22, 9); le16(24, 18); le16(26, 2);
    const std::string sys = argv[1], iface = argv[2];
    assert(candidate(sys + "/sda"));
    auto device = discover(sys.c_str(), "/fake-dev");
    assert(device && !device->readonly && device->blocks == 2880);
    unsigned char data[512] = {};
    unsigned int read_before = reads;
    for (unsigned int i = 0; i < 36; ++i) assert(device->sector(i, data) == Result::ok);
    assert(reads - read_before == 1); // Track zero was cached during discovery; track one needs one command.
    assert(device->sector(1, data, true) == Result::ok && writes == 1);
    attention = 1;
    assert(device->ready() == Result::changed && !device->valid && !claimed);
    auto replacement = discover(sys.c_str(), "/fake-dev");
    assert(replacement && device->sector(1, data, true) == Result::absent && writes == 1);
    replacement.reset(); device.reset();

    capacity = 1440; boot[13] = 2;
    le16(17, 112); le16(19, 1440); le16(22, 3); le16(24, 9);
    device = discover(sys.c_str(), "/fake-dev");
    assert(device && device->sectors == 9 && device->cylinders == 80);
    device.reset(); capacity = 2880; boot[13] = 1;
    le16(17, 224); le16(19, 2880); le16(22, 9); le16(24, 18);

    tab = true;
    device = discover(sys.c_str(), "/fake-dev");
    assert(device && device->readonly);
    assert(device->sector(1, data, true) == Result::protected_disk && writes == 1);
    assert(device->sector(1, data) == Result::ok);
    device.reset(); tab = false;

    device = discover(sys.c_str(), "/fake-dev");
    assert(device && !device->readonly); tab = true;
    assert(device->sector(1, data, true) == Result::protected_disk && !device->valid && !claimed);
    device.reset(); tab = false;

    rdonly_fd = true;
    device = discover(sys.c_str(), "/fake-dev");
    assert(device && device->readonly); device.reset(); rdonly_fd = false;
    mode_failure = true;
    device = discover(sys.c_str(), "/fake-dev");
    assert(device && device->readonly); device.reset(); mode_failure = false;

    attention = 1; // Initial unit attention is acknowledged before mounting.
    device = discover(sys.c_str(), "/fake-dev");
    assert(device); ++sequence;
    unsigned int before = commands;
    assert(device->sector(1, data, true) == Result::changed && commands == before && !claimed);
    device.reset();

    device = discover(sys.c_str(), "/fake-dev");
    assert(device); absent = true;
    assert(device->ready() == Result::absent && !claimed);
    device.reset();
    assert(!discover(sys.c_str(), "/fake-dev")); absent = false;
    capacity = 65536; assert(!discover(sys.c_str(), "/fake-dev")); capacity = 2880;
    le16(11, 1024); assert(!discover(sys.c_str(), "/fake-dev")); le16(11, 512);
    le16(26, 0); assert(!discover(sys.c_str(), "/fake-dev")); le16(26, 2);
    short_read = true; assert(!discover(sys.c_str(), "/fake-dev")); short_read = false;

    memset(boot, 0, sizeof(boot)); // A blank filesystem must remain accessible to FORMAT.
    assert(!discover(sys.c_str(), "/fake-dev"));
    device = discover(sys.c_str(), "/fake-dev", false);
    assert(device && device->heads == 2 && device->sectors == 18 && device->cylinders == 80);
    assert(device->sector(0, data, true) == Result::ok);
    assert(!device->fat_geometry() && device->heads == 2 && device->sectors == 18);
    device.reset();

    // Exercise the actual adapter's FORMAT BIOS requests and DOS IOCTL packets.
    persistent = true;
    le16(11, 512); boot[13] = 1; le16(14, 1); boot[16] = 2;
    le16(17, 224); le16(19, 2880); le16(22, 9); le16(24, 18); le16(26, 2);
    memcpy(media, boot, 512);
    // Metadata comparisons must use committed physical contents, not staged
    // read windows. File data and explicit raw writes are never suppressed.
    device = discover(sys.c_str(), "/fake-dev");
    assert(device->sector(19, data) == Result::ok);
    unsigned char unchanged[512] = {}, changed[512];
    memset(changed, 0x5a, sizeof(changed));
    before = commands; read_before = reads;
    unsigned int metadata_writes = writes;
    assert(device->begin_write());
    assert(device->sector(1, changed, true) == Result::ok);
    assert(device->sector(10, unchanged, true) == Result::ok);
    assert(device->sector(19, unchanged, true) == Result::ok);
    assert(device->sector(36, unchanged, true) == Result::ok);
    assert(device->end_write() == Result::ok);
    assert(writes == metadata_writes + 2 && commands == before + 2 && reads == read_before);
    assert(!memcmp(media + 512, changed, 512) && !memcmp(media + 10 * 512, unchanged, 512));

    before = commands; metadata_writes = writes;
    assert(device->begin_write());
    assert(device->sector(1, changed, true) == Result::ok);
    assert(device->sector(10, unchanged, true) == Result::ok);
    assert(device->sector(19, unchanged, true) == Result::ok);
    assert(device->end_write() == Result::ok);
    assert(writes == metadata_writes && commands == before + 1 && reads == read_before); // One TUR.

    // A change reverted within the operation also needs no physical rewrite.
    assert(device->begin_write());
    assert(device->sector(1, unchanged, true) == Result::ok);
    assert(device->sector(1, changed, true) == Result::ok);
    assert(device->end_write() == Result::ok && writes == metadata_writes);

    // Read a previously uncached track while its directory sector is dirty.
    device->clear_cache();
    assert(device->begin_write());
    assert(device->sector(19, changed, true) == Result::ok);
    assert(device->sector(20, data) == Result::ok);
    assert(device->end_write() == Result::ok && writes == metadata_writes + 1);
    assert(!memcmp(media + 19 * 512, changed, 512));
    assert(device->sector(19, data) == Result::ok && !memcmp(data, changed, 512));
    metadata_writes = writes; before = commands;
    assert(device->raw_io(19, 1, changed, true) == Result::ok);
    assert(writes == metadata_writes + 1 && commands == before + 1);
    assert(device->begin_write()); // Raw access discarded comparison snapshots.
    assert(device->sector(19, changed, true) == Result::ok);
    assert(device->end_write() == Result::ok && writes == metadata_writes + 2);

    // An unchanged sector between changed neighbours stays in the transfer;
    // skipping it must not turn one command into two mechanical operations.
    device->clear_cache();
    assert(device->sector(2, data) == Result::ok);
    metadata_writes = writes; before = commands; largest_write = 0;
    assert(device->begin_write());
    assert(device->sector(2, changed, true) == Result::ok);
    assert(device->sector(3, unchanged, true) == Result::ok);
    assert(device->sector(4, changed, true) == Result::ok);
    assert(device->end_write() == Result::ok);
    assert(writes == metadata_writes + 1 && commands == before + 1 && largest_write == 3);
    assert(!memcmp(media + 2 * 512, changed, 512) && !memcmp(media + 3 * 512, unchanged, 512));

    // Even an entirely unchanged batch must detect replacement media.
    assert(device->sector(19, data) == Result::ok);
    assert(device->begin_write());
    assert(device->sector(19, changed, true) == Result::ok);
    metadata_writes = writes; attention = 1;
    assert(device->end_write() == Result::changed && writes == metadata_writes && !claimed);
    device.reset();
    device = discover(sys.c_str(), "/fake-dev");
    assert(device->sector(19, data) == Result::ok);
    assert(device->begin_write());
    assert(device->sector(19, data, true) == Result::ok);
    ++sequence; before = commands;
    assert(device->end_write() == Result::changed && writes == metadata_writes && commands == before && !claimed);
    device.reset();
    {
        device = discover(sys.c_str(), "/fake-dev");
        auto medium = std::make_shared<Medium>(device);
        File copy(medium, new CopyFile(device));
        unsigned int before_writes = writes, before_commands = commands;
        uint16_t size = 18 * 512;
        assert(copy.Write(data, &size));
        assert(writes == before_writes + 4 && commands == before_commands + 4);
        assert(media[36 * 512] == 1 && media[53 * 512] == 18 && media[512] == 18);
        before_commands = commands;
        for (unsigned int i = 36; i < 54; ++i) {
            assert(device->sector(i, data) == Result::ok);
            assert(device->sector(1, data) == Result::ok);
            assert(device->sector(19, data) == Result::ok);
        }
        assert(commands == before_commands + 2); // Retain FAT/root alongside file data.

        assert(device->begin_write());
        assert(device->sector(1, data, true) == Result::ok);
        assert(device->sector(36, data, true) == Result::ok);
        before_writes = writes;
        attention = 1;
        assert(device->end_write() == Result::changed && writes == before_writes && !claimed);
    }
    device.reset();
    memcpy(media, boot, 512);

    // Sequential file reads combine tracks; short/random reads stay small.
    device = discover(sys.c_str(), "/fake-dev");
    for (unsigned int i = 702; i < 784; ++i) memset(media + i * 512, i % 251, 512);
    read_before = reads; largest_read = 0;
    for (unsigned int i = 702; i < 784; ++i) {
        assert(device->sector(i, data) == Result::ok && data[0] == i % 251);
    }
    assert(reads == read_before + 2 && largest_read == 64);
    largest_read = 0;
    assert(device->sector(900, data) == Result::ok && largest_read <= 18);
    device.reset();

    // A 32-KiB DOS write needs one data command plus FAT copies and directory,
    // not four commands just to split the data at physical track boundaries.
    {
        device = discover(sys.c_str(), "/fake-dev");
        auto medium = std::make_shared<Medium>(device);
        File copy(medium, new CopyFile(device, 64));
        uint16_t size = 64 * 512;
        unsigned int before_writes = writes;
        largest_write = 0;
        assert(copy.Write(data, &size) && size == 64 * 512);
        assert(writes == before_writes + 4 && largest_write == 64);
        for (unsigned int i = 0; i < 64; ++i)
            for (unsigned int b = 0; b < 512; ++b) assert(media[(36 + i) * 512 + b] == i + 1);
    }
    device.reset();

    // The shell's 60-KiB write stays in one DOS batch while the transport
    // splits data into two bounded USB commands and commits metadata once.
    {
        device = discover(sys.c_str(), "/fake-dev");
        auto medium = std::make_shared<Medium>(device);
        File copy(medium, new CopyFile(device, 120));
        uint16_t size = 60 * 1024;
        unsigned int before_writes = writes;
        largest_write = 0;
        assert(copy.Write(data, &size) && size == 60 * 1024);
        assert(writes == before_writes + 5 && largest_write == 64);
        for (unsigned int i = 0; i < 120; ++i)
            for (unsigned int b = 0; b < 512; ++b) assert(media[(36 + i) * 512 + b] == i + 1);
    }
    device.reset();

    // Backwards seeks can create overlapping cache windows. Updating a
    // sector must update every cached copy, including after LRU eviction.
    device = discover(sys.c_str(), "/fake-dev");
    for (unsigned int i = 1000; i <= 1008; ++i) assert(device->sector(i, data) == Result::ok);
    assert(device->sector(990, data) == Result::ok);
    memset(data, 0x91, 512);
    assert(device->sector(1001, data, true) == Result::ok);
    assert(device->sector(990, data) == Result::ok);
    assert(device->sector(1200, data) == Result::ok && device->sector(1300, data) == Result::ok);
    read_before = reads;
    assert(device->sector(1001, data) == Result::ok && data[0] == 0x91 && reads == read_before);
    device.reset();

    // Legacy firmware can reject crossing a track. Fall back on that medium
    // for both reads and writes without retrying a changed/replaced disk.
    reject_cross_track = true;
    device = discover(sys.c_str(), "/fake-dev");
    unsigned int rejected_before = rejected;
    largest_read = 0;
    for (unsigned int i = 702; i < 784; ++i)
        assert(device->sector(i, data) == Result::ok && data[0] == i % 251);
    assert(rejected == rejected_before + 1 && largest_read <= 18);
    device.reset();
    {
        device = discover(sys.c_str(), "/fake-dev");
        auto medium = std::make_shared<Medium>(device);
        File copy(medium, new CopyFile(device, 64));
        uint16_t size = 64 * 512;
        rejected_before = rejected; largest_write = 0;
        assert(copy.Write(data, &size) && largest_write <= 18 && rejected == rejected_before + 1);
        assert(media[99 * 512] == 64);
    }
    device.reset();
    device = discover(sys.c_str(), "/fake-dev");
    unsigned char bulk[64 * 512] = {};
    unsigned int before_writes = writes;
    change_on_reject = true;
    assert(device->raw_io(36, 64, bulk, true) == Result::changed && writes == before_writes && !claimed);
    device.reset(); reject_cross_track = change_on_reject = false;

    // Respect small Linux queue limits, and recover automatically when a
    // controller rejects an oversized SG_IO with EINVAL rather than sense.
    FILE *limit = fopen((sys + "/sda/queue/max_sectors_kb").c_str(), "w");
    assert(limit); fputs("4\n", limit); fclose(limit);
    largest_read = 0;
    device = discover(sys.c_str(), "/fake-dev");
    assert(device && largest_read <= 8);
    largest_write = 0;
    assert(device->raw_io(36, 64, bulk, true) == Result::ok && largest_write <= 8);
    device.reset();
    limit = fopen((sys + "/sda/queue/max_sectors_kb").c_str(), "w");
    assert(limit); fputs("32\n", limit); fclose(limit);
    command_limit = 8; reject_errno = true; largest_read = 0;
    device = discover(sys.c_str(), "/fake-dev");
    assert(device && largest_read <= 8);
    assert(device->raw_io(36, 64, bulk, true) == Result::ok);
    device.reset(); command_limit = 64; reject_errno = false;
    memcpy(media, boot, 512);

    device = discover(sys.c_str(), "/fake-dev");
    // Fetching a track after staging one of its sectors must overlay that
    // pending sector, so cached reads stay correct after commit.
    assert(device->begin_write());
    memset(data, 0x9b, sizeof(data));
    assert(device->sector(72, data, true) == Result::ok);
    assert(device->sector(73, data) == Result::ok);
    assert(device->end_write() == Result::ok);
    assert(device->sector(72, data) == Result::ok && data[0] == 0x9b);
    device.reset();

    // Blocking USB work services emulator events, preserves IF and waits for
    // outstanding buffers before propagating a quit/reset exception.
    device = discover(sys.c_str(), "/fake-dev");
    io_executor = responsive_io;
    latency_ms = 5;
    assert(device->raw_io(36, 1, data, false) == Result::ok && idle_calls && interrupt_flag);
    interrupt_flag = false;
    assert(device->ready() == Result::ok && !interrupt_flag);
    interrupt_flag = true; idle_throws = true;
    bool caught = false;
    try { device->ready(); } catch (int value) { caught = value == 123; }
    assert(caught && interrupt_flag);
    caught = false;
    try {
        WriteBatch batch(device);
        assert(device->sector(36, data, true) == Result::ok);
        batch.finish();
    } catch (int value) { caught = value == 123; }
    assert(caught && !device->valid && !claimed && interrupt_flag);
    idle_throws = false; latency_ms = 0; io_executor = nullptr;
    device.reset();

    auto controller = std::make_shared<Controller>();
    {
        Drive floppy(controller);
        assert(controller->ready(true));
        auto old = controller->medium;
        File handle(old, new DOS_File);
        uint16_t size = 1;
        assert(handle.Read(data, &size));
        reg_dl = 0; reg_ah = 0x18; reg_ch = 79; reg_cl = 18;
        assert(PI_DOS_USBFloppyBIOS() && !carry && reg_ah == 0);
        reg_ah = 0x18; reg_cl = 9;
        assert(PI_DOS_USBFloppyBIOS() && carry && reg_ah == 0x0c);
        es = 0; reg_bx = 0x1000; reg_ah = 5; reg_al = 18; reg_ch = 0; reg_dh = 0;
        for (unsigned int i = 0; i < 18; ++i) {
            guest[0x1000 + i * 4] = 0; guest[0x1001 + i * 4] = 0;
            guest[0x1002 + i * 4] = i + 1; guest[0x1003 + i * 4] = 2;
        }
        unsigned int before_writes = writes;
        assert(PI_DOS_USBFloppyBIOS() && !carry && writes == before_writes + 1);
        assert(media[0] == 0xf6 && media[18 * 512 - 1] == 0xf6);
        reg_ah = 2; reg_cl = 1; reg_bx = 0x4000;
        unsigned int before_commands = commands, before_reads = reads;
        assert(PI_DOS_USBFloppyBIOS() && carry && reg_ah == 6 && commands == before_commands);
        reg_ah = 2;
        assert(PI_DOS_USBFloppyBIOS() && !carry && reg_ah == 0);
        assert(commands == before_commands + 1 && reads == before_reads + 1);
        assert(guest[0x4000] == 0xf6 && guest[0x4000 + 18 * 512 - 1] == 0xf6);

        // A whole BIOS write/read/verify request uses one physical transfer,
        // with no per-sector readiness command or emulated-delay fallback.
        reg_ah = 3; reg_ch = 1; reg_dh = 0; reg_cl = 1;
        memset(guest + 0x4000, 0xcd, 18 * 512);
        before_commands = commands; before_writes = writes;
        assert(PI_DOS_USBFloppyBIOS() && !carry && commands == before_commands + 1);
        assert(writes == before_writes + 1 && media[36 * 512] == 0xcd);
        memset(guest + 0x4000, 0, 18 * 512);
        reg_ah = 2; before_commands = commands;
        assert(PI_DOS_USBFloppyBIOS() && !carry && commands == before_commands + 1);
        assert(guest[0x4000] == 0xcd && guest[0x4000 + 18 * 512 - 1] == 0xcd);
        reg_ah = 4; before_commands = commands;
        assert(PI_DOS_USBFloppyBIOS() && !carry && commands == before_commands + 1);
        reg_ah = 2; reg_ch = 79; reg_dh = 1; reg_cl = 18; reg_al = 2;
        before_commands = commands;
        assert(PI_DOS_USBFloppyBIOS() && carry && commands == before_commands);
        reg_al = 18; reg_ch = 0; reg_dh = 0;
        assert(!old->device->valid && controller->medium != old && !controller->medium->fat);
        assert(!handle.Read(data, &size) && !handle.Write(data, &size));
        before_writes = writes; assert(handle.Close() && writes == before_writes);
        assert(!controller->ready(true)); // FORMAT's track fill has no valid FAT yet.
        assert(floppy.Write_AbsoluteSector_INT25(0, boot) == 0);
        assert(controller->ready(true)); // A newly written BPB is mounted afresh.

        // Manual eject/insert needs no unmount, even inside the probe debounce interval.
        DOS_DTA dta;
        auto swapped = controller->medium;
        File stale(swapped, new DOS_File);
        attention = 1;
        floppy.FindFirst("", dta);
        assert(controller->medium != swapped && !swapped->device->valid);
        assert(!stale.Read(data, &size));
        absent = true;
        floppy.FindFirst("", dta);
        assert(!controller->medium && !claimed);
        absent = false;
        floppy.FindFirst("", dta);
        assert(controller->medium && controller->medium->fat && claimed);

        // FORMAT uses AH=32h to decide whether /Q may reuse an existing FAT.
        // Exercise the actual patched DOS dispatcher with the real adapter.
        Drives[0] = &floppy;
        before_writes = writes;
        unsigned int before_dpb = dpb_updates;
        reg_ah = 0x32; reg_dl = 1; reg_al = 0xff; ds = 0; reg_bx = 0xffff;
        dpb_handler();
        assert(reg_al == 0 && ds == dos.tables.dpb && reg_bx == 0);
        assert(dpb_updates == before_dpb + 1 && dpb_drive == 0 && writes == before_writes);
        reg_ah = 0x32; reg_dl = 0; dpb_handler(); // Default drive.
        assert(reg_al == 0);
        reg_ah = 0x1f; reg_dl = 255; dpb_handler(); // AH=1Fh always uses default.
        assert(reg_al == 0);
        default_drive = 2; reg_ah = 0x32; reg_dl = 0; dpb_handler();
        assert(reg_al == 0xff); default_drive = 0;
        reg_dl = 27; dpb_handler(); assert(reg_al == 0xff);
        assert(!PI_DOS_USBFloppyDPB(&floppy, 255));
        absent = true; reg_dl = 1; dpb_handler();
        assert(reg_al == 0xff && !controller->medium && writes == before_writes);
        absent = false;
        // A new disk with 720 KiB geometry must replace the previous FAT/DPB.
        capacity = 1440; boot[13] = 2;
        le16(17, 112); le16(19, 1440); le16(22, 3); le16(24, 9);
        memcpy(media, boot, 512);
        dpb_handler();
        assert(reg_al == 0 && controller->medium->device->blocks == 1440);
        assert(controller->medium->device->sectors == 9 && writes == before_writes);
        capacity = 2880; boot[13] = 1;
        le16(17, 224); le16(19, 2880); le16(22, 9); le16(24, 18);
        memcpy(media, boot, 512); ++sequence; dpb_handler(); assert(reg_al == 0);
        // /Q writes filesystem metadata without filling the file-data tracks.
        memset(media + 36 * 512, 0x6e, 512);
        assert(floppy.Write_AbsoluteSector_INT25(0, boot) == 0);
        assert(media[36 * 512] == 0x6e);
        dpb_handler(); assert(reg_al == 0);
        unsigned char blank_boot[512] = {};
        assert(floppy.Write_AbsoluteSector_INT25(0, blank_boot) == 0);
        before_dpb = dpb_updates; dpb_handler();
        assert(reg_al == 0xff && dpb_updates == before_dpb); // Blank disks still need FORMAT.
        assert(floppy.Write_AbsoluteSector_INT25(0, boot) == 0);
        dpb_handler(); assert(reg_al == 0);
        Drives[0] = nullptr;

        const PhysPt packet = 0x2000, buffer = 0x3000;
        assert(PI_DOS_USBFloppyIoctl(&floppy, 8, 0x60, packet, false));
        assert(mem_readw(packet + 7) == 512 && mem_readw(packet + 0xf) == 2880);
        before_writes = writes;
        assert(PI_DOS_USBFloppyIoctl(&floppy, 8, 0x40, packet, false) && writes == before_writes);
        memset(guest + packet, 0, 32);
        mem_writew(packet + 1, 1); mem_writew(packet + 7, 1); mem_writed(packet + 9, buffer);
        memset(guest + buffer, 0xab, 512);
        assert(PI_DOS_USBFloppyIoctl(&floppy, 8, 0x41, packet, false));
        memset(guest + buffer, 0, 512);
        assert(PI_DOS_USBFloppyIoctl(&floppy, 8, 0x61, packet, false));
        assert(guest[buffer] == 0xab && guest[buffer + 511] == 0xab);
        mem_writew(packet + 3, 79); mem_writeb(packet, 1); mem_writew(packet + 5, 2);
        before_writes = writes;
        assert(!PI_DOS_USBFloppyIoctl(&floppy, 8, 0x42, packet, false) && writes == before_writes);
        controller->medium->device->readonly = true;
        reg_dl = 0; reg_ah = 5;
        assert(PI_DOS_USBFloppyBIOS() && carry && reg_ah == 3 && writes == before_writes);
        reg_dl = 0x80; reg_ah = 5;
        assert(!PI_DOS_USBFloppyBIOS()); // Never intercept hard-disk formatting.
        Program *program = nullptr; pidos_floppy::program(&program); delete program;
    }
    controller.reset();
    assert(!claimed && !imageDiskList[0]);

    // The expanded diagnostics must stay opt-in, including sysfs metadata.
    assert(setenv("PI_DOS_DIAGNOSTICS", "0", 1) == 0);
    device = discover(sys.c_str(), "/silent");
    assert(device);
    device.reset();

    FILE *file = fopen((iface + "/bInterfaceSubClass").c_str(), "w");
    assert(file); fputs("06\n", file); fclose(file);
    assert(!candidate(sys + "/sda")); // Ordinary removable USB stick.
    file = fopen((sys + "/sda/device/model").c_str(), "w");
    assert(file); fputs("USB FLOPPY\n", file); fclose(file);
    assert(candidate(sys + "/sda")); // Named legacy floppy using transparent SCSI.
    file = fopen((sys + "/sda/partition").c_str(), "w");
    assert(file); fputs("1\n", file); fclose(file);
    assert(!candidate(sys + "/sda"));
    assert(!claimed);
}
''')
    for name in ('linux', 'dos'):
        content = (ROOT / f'src/runtime/usb_floppy_{name}.h').read_text()
        if name == 'dos':
            content = content.replace('"/sys/class/block"', f'"{block}"').replace('"/dev"', '"/fake-dev"')
        (work / f'pidos_usb_floppy_{name}.h').write_text(content)
    (work / 'usb-floppy-test-dos.h').write_bytes((ROOT / 'tools/image/usb-floppy-test-dos.h').read_bytes())
    executable = work / 'check'
    subprocess.run(['g++', '-std=c++11', '-DLINUX=1', '-Wall', '-Wextra', '-Werror', '-pthread',
                    '-I', str(ROOT / 'src/runtime'), str(source), '-o', str(executable)], check=True)
    checked = subprocess.run([str(executable), str(block), str(interface)],
                             capture_output=True, text=True,
                             env={**os.environ, 'PI_DOS_DIAGNOSTICS': '1'})
    if checked.returncode:
        print(checked.stderr[-2500:])
        checked.check_returncode()
    assert 'op=28 lba=0 bytes=9216' in checked.stderr
    assert 'op=2a lba=0 bytes=9216' in checked.stderr
    assert 'gap_ms=' in checked.stderr and 'elapsed_ms=' in checked.stderr and 'host=' in checked.stderr
    assert 'trace_version=2' in checked.stderr and 'vendor=0644 product=0000 revision=0000 speed_mbps=12' in checked.stderr
    assert 'subclass=04 protocol=00' in checked.stderr and 'power_control=unknown' in checked.stderr
    assert 'USB floppy DPB: drive=0 valid=1' in checked.stderr
    assert 'USB floppy DPB: drive=0 valid=0' in checked.stderr
    assert 'unchanged_metadata=2' in checked.stderr and 'unchanged_metadata=3' in checked.stderr
    assert 'bInterval_hex=ff descriptor_interval=255ms' in checked.stderr
    assert '/silent/' not in checked.stderr
    timings = []
    for record in checked.stderr.splitlines():
        if not record.startswith('Pi-DOS USB floppy:'):
            continue
        values = dict(re.findall(r'(\w+)=(\S+)', record))
        for key in ('elapsed_ms', 'check_us', 'queue_us', 'ioctl_us', 'resume_us', 'sg_ms'):
            values[key] = int(values[key])
            assert values[key] >= 0
        total_us = values['queue_us'] + values['ioctl_us'] + values['resume_us']
        assert abs(total_us - values['elapsed_ms'] * 1000) <= 1002
        timings.append(values)
    delayed = [record for record in timings if record['sg_ms'] == 5]
    assert delayed and all(record['ioctl_us'] >= 5000 for record in delayed)
    assert any(record['region'] == 'data' for record in timings)
    assert any(record['region'] == 'fat' for record in timings)

print('PASS: classification, FAT12/raw media, bounded multi-track I/O and rejection fallback, overlapping caches, unchanged metadata/media guards, write coalescing/commit, responsive I/O, diagnostic timings/USB descriptors, BIOS FORMAT geometry/track requests, '
      'DOS sector/IOCTL access, retired file handles, write protection, bounds and disk changes.')
