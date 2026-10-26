#define _GNU_SOURCE
#include "network.h"
#include <errno.h>
#include <fcntl.h>
#include <pwd.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

static pid_t launch(char *const arguments[], int log_fd)
{
    pid_t child = fork();
    if (!child) {
        if (log_fd >= 0) {
            if (dup2(log_fd, STDOUT_FILENO) < 0 || dup2(log_fd, STDERR_FILENO) < 0)
                _exit(127);
            close(log_fd);
        }
        sigset_t empty;
        sigemptyset(&empty);
        sigprocmask(SIG_SETMASK, &empty, NULL);
        setsid();
        execv(arguments[0], arguments);
        dprintf(2, "Pi-DOS: %s exec failed: %s\n", arguments[0], strerror(errno));
        _exit(127);
    }
    if (child < 0)
        dprintf(2, "Pi-DOS: %s fork failed: %s\n", arguments[0], strerror(errno));
    return child;
}

void pi_dos_network_start(struct pi_dos_network *network, bool capture_log)
{
    int log_fd = -1;
    if (capture_log) {
        log_fd = open("/dosdata/c/SYSTEM/NETWORK.LOG",
                      O_WRONLY | O_CREAT | O_TRUNC | O_APPEND | O_CLOEXEC | O_NOFOLLOW, 0644);
        if (log_fd < 0)
            dprintf(2, "Pi-DOS: cannot create NETWORK.LOG: %s\n", strerror(errno));
        else
            dprintf(log_fd, "Pi-DOS wired DHCP and network time output\n");
    }
    mkdir("/run/dhcpcd", 0755);
    mkdir("/run/dhcpcd-state", 0755);
    mkdir("/run/chrony", 0755);
    struct passwd *user = getpwnam("chrony");
    if (user && chown("/run/chrony", user->pw_uid, user->pw_gid))
        dprintf(2, "Pi-DOS: chrony directory ownership failed: %s\n", strerror(errno));
    /* Keep both daemons in the foreground under PID 1. No cable, DHCP or DNS
     * wait can hold up the DOS prompt; DOSBox follows later clock corrections. */
    char *dhcp[] = {"/sbin/dhcpcd", "-4", "-B", "-t", "0",
                    "-f", "/etc/dhcpcd.conf", NULL};
    char *clock[] = {"/usr/sbin/chronyd", "-4", "-d",
                     "-f", "/etc/chrony.conf", NULL};
    network->dhcp = launch(dhcp, log_fd);
    network->clock = launch(clock, log_fd);
    if (log_fd >= 0) close(log_fd);
    dprintf(2, "Pi-DOS: wired DHCP and network time started asynchronously\n");
}

void pi_dos_network_reaped(struct pi_dos_network *network, pid_t child, int status)
{
    const char *service = NULL;
    if (child == network->dhcp) {
        service = "DHCP";
        network->dhcp = -1;
    } else if (child == network->clock) {
        service = "network time";
        network->clock = -1;
    }
    if (service)
        dprintf(2, "Pi-DOS: %s service exited (wait status %d)\n", service, status);
}

void pi_dos_network_stop(struct pi_dos_network *network)
{
    pid_t children[] = {network->dhcp, network->clock};
    for (size_t i = 0; i < 2; ++i)
        if (children[i] > 0) kill(children[i], SIGTERM);
    /* Bound shutdown without using wall time, which NTP can change. */
    for (int attempt = 0; attempt < 20; ++attempt) {
        bool pending = false;
        for (size_t i = 0; i < 2; ++i) {
            if (children[i] <= 0) continue;
            pid_t result = waitpid(children[i], NULL, WNOHANG);
            if (result == children[i] || (result < 0 && errno == ECHILD))
                children[i] = -1;
            else pending = true;
        }
        if (!pending) break;
        struct timespec delay = {0, 100000000};
        nanosleep(&delay, NULL);
    }
    for (size_t i = 0; i < 2; ++i)
        if (children[i] > 0) kill(children[i], SIGKILL);
}
