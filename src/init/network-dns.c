#define _POSIX_C_SOURCE 200809L
/* dhcpcd configures the interface and routes. This native hook supplies DNS
 * without a shell, resolvconf, or writes to the read-only root filesystem. */
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#ifndef PI_DOS_RESOLV_PATH
#define PI_DOS_RESOLV_PATH "/run/resolv.conf"
#endif

int main(void)
{
    const char *reason = getenv("reason");
    const char *servers = getenv("new_domain_name_servers");
    if (!reason || !servers || !*servers) return 0;
    if (strcmp(reason, "BOUND") && strcmp(reason, "RENEW") &&
        strcmp(reason, "REBIND") && strcmp(reason, "REBOOT") &&
        strcmp(reason, "INFORM") && strcmp(reason, "STATIC")) return 0;

    char *list = strdup(servers);
    if (!list) return 1;
    FILE *output = fopen(PI_DOS_RESOLV_PATH ".tmp", "w");
    if (!output) { free(list); return 1; }
    fputs("# DNS supplied by wired DHCP\n", output);
    char *save = NULL;
    unsigned int count = 0;
    for (char *item = strtok_r(list, " \t", &save); item && count < 3;
         item = strtok_r(NULL, " \t", &save)) {
        struct in_addr address;
        if (inet_pton(AF_INET, item, &address) != 1) continue;
        fprintf(output, "nameserver %s\n", item);
        ++count;
    }
    free(list);
    int failed = ferror(output);
    if (fclose(output)) failed = 1;
    if (!failed && count) {
        if (!rename(PI_DOS_RESOLV_PATH ".tmp", PI_DOS_RESOLV_PATH)) return 0;
        failed = 1;
    }
    unlink(PI_DOS_RESOLV_PATH ".tmp");
    return failed ? 1 : 0;
}
