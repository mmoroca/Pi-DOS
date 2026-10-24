/* Host fallback: exercise the production FAT32 resizer on regular files only. */
#include "../../src/init/storage.c"

int main(int argc, char **argv)
{
    struct stat status;
    unsigned char mbr[512], bpb[512];
    if (argc != 2 || stat(argv[1], &status) || !S_ISREG(status.st_mode)) return 2;
    int fd = open(argv[1], O_RDONLY | O_CLOEXEC);
    if (fd < 0) return 2;
    bool ok = pread(fd, mbr, sizeof(mbr), 0) == sizeof(mbr);
    uint64_t start = ok ? le32(mbr + 470) : 0;
    ok = ok && le32(mbr + 440) == 0xa4d05001 && mbr[466] == 0x0c &&
         start == 526336 && status.st_size > (off_t)(start * 512) &&
         pread(fd, bpb, sizeof(bpb), (off_t)(start * 512)) == sizeof(bpb);
    close(fd);
    if (!ok || memcmp(bpb + 71, "DOSDATA    ", 11)) return 2;
    uint64_t old = le32(bpb + 32), wanted = (uint64_t)status.st_size / 512 - start;
    if (old <= wanted && wanted - old < bpb[13]) {
        puts("Expansion result: 0");
        return 0;
    }
    if (old > wanted || resize_fat(argv[1], start, old, wanted)) return 1;
    puts("Expansion result: 1");
    return 0;
}
