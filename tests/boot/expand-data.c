/* Disposable loop-device integration check; never included in the SD image. */
#include "storage.h"
#include <stdio.h>
#include <string.h>

int main(int argc, char **argv)
{
    if (argc != 2 || strncmp(argv[1], "/dev/loop", 9)) {
        fputs("Only a disposable /dev/loop... partition is accepted.\n", stderr);
        return 2;
    }
    int result = armdos_expand_data(argv[1]);
    printf("Expansion result: %d\n", result);
    return result < 0 ? 1 : 0;
}
