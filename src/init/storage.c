#define _GNU_SOURCE
/* Original project code; license selection remains pending. */
#include "storage.h"
#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <limits.h>
#include <linux/fs.h>
#include <libfdisk/libfdisk.h>
#include <parted/parted.h>
#include <signal.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/sysmacros.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#ifndef ARMDOS_FSCK_FAT
#define ARMDOS_FSCK_FAT "/sbin/fsck.fat"
#endif

static uint16_t le16(const unsigned char *p)
{
    return (uint16_t)p[0] | (uint16_t)p[1] << 8;
}

static uint32_t le32(const unsigned char *p)
{
    return (uint32_t)p[0] | (uint32_t)p[1] << 8 |
           (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24;
}

static bool fat32_info(const char *device, uint64_t *sectors, unsigned *cluster)
{
    unsigned char boot[512];
    int fd = open(device, O_RDONLY | O_CLOEXEC);
    if (fd < 0) return false;
    ssize_t count = pread(fd, boot, sizeof(boot), 0);
    close(fd);
    if (count != sizeof(boot) || boot[510] != 0x55 || boot[511] != 0xaa ||
        le16(boot + 11) != 512 || !boot[13] || boot[13] > 128 ||
        (boot[13] & (boot[13] - 1)) || !le16(boot + 14) || boot[16] != 2 ||
        le16(boot + 17) || le16(boot + 19) || le16(boot + 22) ||
        le16(boot + 42) || le32(boot + 44) < 2 ||
        memcmp(boot + 71, "DOSDATA    ", 11)) return false;
    uint64_t total = le32(boot + 32), fat = le32(boot + 36);
    uint64_t overhead = le16(boot + 14) + 2 * fat;
    if (!fat || total <= overhead) return false;
    uint64_t clusters = (total - overhead) / boot[13];
    if (clusters < 65525 || clusters > 0x0ffffff5 || fat * 128 < clusters + 2) return false;
    *sectors = total;
    *cluster = boot[13];
    return true;
}

bool armdos_is_fat32_data(const char *device)
{
    uint64_t sectors;
    unsigned cluster;
    return fat32_info(device, &sectors, &cluster);
}

static int wait_operation(pid_t child, const char *name)
{
    /* A failed utility must not leave first boot waiting indefinitely. */
    struct timespec start, now, pause = {0, 100000000};
    clock_gettime(CLOCK_MONOTONIC, &start);
    for (;;) {
        int status;
        pid_t result = waitpid(child, &status, WNOHANG);
        if (result == child) return WIFEXITED(status) ? WEXITSTATUS(status) : -1;
        if (result < 0 && errno != EINTR) return -1;
        clock_gettime(CLOCK_MONOTONIC, &now);
        if (now.tv_sec - start.tv_sec >= 300) {
            kill(child, SIGKILL);
            while (waitpid(child, &status, 0) < 0 && errno == EINTR) {}
            dprintf(2, "Pi-DOS: %s timed out; DOSDATA will not be mounted writable\n", name);
            return -1;
        }
        nanosleep(&pause, NULL);
    }
}

static void child_setup(void)
{
    sigset_t empty;
    sigemptyset(&empty);
    sigprocmask(SIG_SETMASK, &empty, NULL);
    int null = open("/dev/null", O_RDONLY);
    if (null >= 0) {
        dup2(null, STDIN_FILENO);
        if (null > 2) close(null);
    }
    setenv("LC_ALL", "C", 1);
}

static int check_fat(const char *device)
{
    pid_t child = fork();
    if (child < 0) return -1;
    if (!child) {
        child_setup();
        execl(ARMDOS_FSCK_FAT, ARMDOS_FSCK_FAT, "-n", device, (char *)NULL);
        _exit(127);
    }
    return wait_operation(child, "FAT32 check");
}

static PedExceptionOption fat_exception(PedException *exception)
{
    /* libparted offers FAT16 as an alternative below 2 GiB. This appliance
     * always retains FAT32, including on smaller cards used for testing. */
    if (exception->type == PED_EXCEPTION_INFORMATION &&
        (exception->options & PED_EXCEPTION_YES) &&
        strstr(exception->message, "Would you like to use FAT32?")) {
        dprintf(2, "Pi-DOS: retaining FAT32 during expansion\n");
        return PED_EXCEPTION_YES;
    }
    dprintf(2, "Pi-DOS: FAT32 resize: %s\n", exception->message);
    /* There is no interactive host console. Decline unexpected repairs or
     * format conversions rather than guessing an affirmative response. */
    if (exception->options & PED_EXCEPTION_CANCEL) return PED_EXCEPTION_CANCEL;
    if (exception->options & PED_EXCEPTION_NO) return PED_EXCEPTION_NO;
    if (exception->options & PED_EXCEPTION_OK) return PED_EXCEPTION_OK;
    return PED_EXCEPTION_UNHANDLED;
}

static int resize_fat(const char *disk, uint64_t start, uint64_t old_size, uint64_t new_size)
{
    pid_t child = fork();
    if (child < 0) return -1;
    if (!child) {
        child_setup();
        ped_exception_set_handler(fat_exception);
        PedDevice *device = ped_device_get(disk);
        PedGeometry *old = device ? ped_geometry_new(device, (PedSector)start, (PedSector)old_size) : NULL;
        PedGeometry *grown = device ? ped_geometry_new(device, (PedSector)start, (PedSector)new_size) : NULL;
        PedFileSystem *fs = old ? ped_file_system_open(old) : NULL;
        bool ok = fs && grown && !strcmp(fs->type->name, "fat32");
        PedConstraint *constraint = ok ? ped_file_system_get_resize_constraint(fs) : NULL;
        PedGeometry *fit = constraint ? ped_constraint_solve_nearest(constraint, grown) : NULL;
        /* libparted may reserve the final sector. Permit less than one maximum
         * FAT32 cluster of padding, but never move the filesystem start. */
        ok = fit && fit->start == (PedSector)start && fit->length <= (PedSector)new_size &&
             new_size - (uint64_t)fit->length < 128;
        if (!ok) dprintf(2, "Pi-DOS: no supported FAT32 growth geometry\n");
        if (ok) ok = ped_file_system_resize(fs, fit, NULL) != 0;
        if (fit) ped_geometry_destroy(fit);
        if (constraint) ped_constraint_destroy(constraint);
        if (fs && !ped_device_sync(device)) ok = false;
        if (fs && !ped_file_system_close(fs)) ok = false;
        if (old) ped_geometry_destroy(old);
        if (grown) ped_geometry_destroy(grown);
        if (device) ped_device_destroy(device);
        _exit(ok ? 0 : 1);
    }
    return wait_operation(child, "FAT32 expansion");
}

static bool partition_bytes(const char *device, uint64_t *bytes)
{
    int fd = open(device, O_RDONLY | O_CLOEXEC);
    if (fd < 0) return false;
    bool ok = ioctl(fd, BLKGETSIZE64, bytes) == 0;
    close(fd);
    return ok;
}

int armdos_expand_data(const char *device)
{
    struct stat part_stat, disk_stat;
    char link[128], sys_part[PATH_MAX], disk[PATH_MAX];
    unsigned char mbr[512];
    uint64_t bytes, part_bytes;
    if (stat(device, &part_stat) || !S_ISBLK(part_stat.st_mode)) return 0;
    snprintf(link, sizeof(link), "/sys/dev/block/%u:%u",
             major(part_stat.st_rdev), minor(part_stat.st_rdev));
    if (!realpath(link, sys_part)) return 0;
    char *slash = strrchr(sys_part, '/');
    if (!slash) return 0;
    *slash = 0;
    char *parent = strrchr(sys_part, '/');
    if (!parent || snprintf(disk, sizeof(disk), "/dev/%s", parent + 1) >= (int)sizeof(disk)) return 0;
    /* Accept partition 2 of the image's own parent disk, never a guessed disk. */
    char sys_index[PATH_MAX], index[16];
    snprintf(sys_index, sizeof(sys_index), "%s/partition", link);
    FILE *file = fopen(sys_index, "r");
    if (!file) return 0;
    bool second = fgets(index, sizeof(index), file) && !strcmp(index, "2\n");
    fclose(file);
    if (!second || stat(disk, &disk_stat) || !S_ISBLK(disk_stat.st_mode) ||
        disk_stat.st_rdev == part_stat.st_rdev) return 0;

    uint64_t fat_sectors;
    unsigned cluster_size;
    if (!fat32_info(device, &fat_sectors, &cluster_size)) return 0;
    int fd = open(disk, O_RDONLY | O_CLOEXEC);
    if (fd < 0) return -1;
    int sector_size = 0;
    bool ok = pread(fd, mbr, sizeof(mbr), 0) == sizeof(mbr) &&
              !ioctl(fd, BLKGETSIZE64, &bytes) &&
              !ioctl(fd, BLKSSZGET, &sector_size);
    close(fd);
    if (!ok || !partition_bytes(device, &part_bytes)) return -1;
    const unsigned char *boot = mbr + 446, *data = boot + 16;
    uint64_t start = le32(data + 8), sectors = le32(data + 12);
    uint64_t disk_sectors = bytes / 512;
    /* This release generates only this MBR layout. Reject GPT, extra partitions,
     * mismatched kernel geometry and cards beyond MBR's addressable range. */
    if (sector_size != 512 || bytes % 512 || disk_sectors > UINT32_MAX ||
        mbr[510] != 0x55 || mbr[511] != 0xaa || le32(mbr + 440) != 0xa4d05001 ||
        boot[0] != 0x80 || boot[4] != 0x0c || le32(boot + 8) != 2048 ||
        le32(boot + 12) != 524288 || data[0] != 0 || data[4] != 0x0c ||
        start != 526336 || !sectors || start + sectors > disk_sectors ||
        part_bytes != sectors * 512 || memcmp(mbr + 478, (unsigned char[32]){0}, 32)) {
        dprintf(2, "Pi-DOS: automatic DOSDATA expansion skipped: nonstandard disk layout\n");
        return 0;
    }
    if (fat_sectors > sectors) return -1;
    uint64_t wanted = disk_sectors - start;
    if (sectors == wanted && wanted - fat_sectors < cluster_size) return 0;

    dprintf(2, "Pi-DOS: expanding DOSDATA to remaining card space (%" PRIu64 " MiB)\n",
            wanted * 512 / 1048576);
    int checked = check_fat(device);
    if (checked != 0) {
        dprintf(2, "Pi-DOS: DOSDATA check failed (%d); expansion stopped\n", checked);
        return -1;
    }
    if (sectors < wanted) {
        struct fdisk_context *context = fdisk_new_context();
        struct fdisk_partition *partition = NULL;
        if (!context) return -1;
        fdisk_disable_dialogs(context, 1);
        int result = fdisk_assign_device(context, disk, 0);
        if (!result && (!fdisk_is_label(context, DOS) || fdisk_get_sector_size(context) != 512)) result = -1;
        if (!result) result = fdisk_get_partition(context, 1, &partition);
        if (!result && (fdisk_partition_get_start(partition) != start ||
                        fdisk_partition_get_size(partition) != sectors)) result = -1;
        if (!result) result = fdisk_partition_set_size(partition, wanted);
        if (!result) result = fdisk_partition_size_explicit(partition, 1);
        if (!result) result = fdisk_set_partition(context, 1, partition);
        if (!result) result = fdisk_write_disklabel(context);
        if (!result) result = fdisk_reread_partition_table(context);
        if (partition) fdisk_unref_partition(partition);
        fdisk_deassign_device(context, 0);
        fdisk_unref_context(context);
        if (result) {
            dprintf(2, "Pi-DOS: DOSDATA partition growth failed; retry on next boot\n");
            return -1;
        }
        /* Rereading can briefly remove and recreate the devtmpfs node. */
        for (int retry = 0; retry < 20; ++retry) {
            if (partition_bytes(device, &part_bytes) && part_bytes == wanted * 512) break;
            struct timespec pause = {0, 100000000};
            nanosleep(&pause, NULL);
        }
        if (part_bytes != wanted * 512) return -1;
    }
    if (resize_fat(disk, start, fat_sectors, wanted) != 0 ||
        !fat32_info(device, &fat_sectors, &cluster_size) ||
        fat_sectors > wanted || wanted - fat_sectors >= cluster_size || check_fat(device) != 0) {
        dprintf(2, "Pi-DOS: DOSDATA filesystem growth failed; retry on next boot\n");
        return -1;
    }
    dprintf(2, "Pi-DOS: DOSDATA expansion complete\n");
    return 1;
}
