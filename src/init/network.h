#ifndef PI_DOS_NETWORK_H
#define PI_DOS_NETWORK_H

#include <stdbool.h>
#include <sys/types.h>

struct pi_dos_network {
    pid_t dhcp;
    pid_t clock;
};

void pi_dos_network_start(struct pi_dos_network *network, bool capture_log);
void pi_dos_network_reaped(struct pi_dos_network *network, pid_t child, int status);
void pi_dos_network_stop(struct pi_dos_network *network);

#endif
