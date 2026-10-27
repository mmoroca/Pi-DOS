#ifndef PI_DOS_POWER_BUTTON_H
#define PI_DOS_POWER_BUTTON_H
#include <stdbool.h>
#include <time.h>

struct pi_dos_power_button {
    int fd;
    bool down, dropped;
    time_t next_scan;
};

void pi_dos_power_button_scan(struct pi_dos_power_button *button);
bool pi_dos_power_button_read(struct pi_dos_power_button *button);
void pi_dos_power_button_close(struct pi_dos_power_button *button);
#endif
