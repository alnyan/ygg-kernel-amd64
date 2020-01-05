#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <time.h>

int main(int argc, char **argv) {
    const char *arg = NULL;

    if (argc == 2) {
        arg = argv[1];
    }

    int res;
    unsigned int cmd = YGG_REBOOT_RESTART;

    if (arg) {
        if (!strcmp(arg, "-s")) {
            cmd = YGG_REBOOT_POWER_OFF;
        } else if (!strcmp(arg, "-h")) {
            cmd = YGG_REBOOT_HALT;
        }
    }

    if ((res = reboot(YGG_REBOOT_MAGIC1, YGG_REBOOT_MAGIC2, cmd, NULL)) < 0) {
        perror("reboot()");
        return res;
    }

    while (1) {
        usleep(1000000);
    }
}
