#include "arch/amd64/syscall.h"
#include "drivers/pci/pci.h"
#include "drivers/usb/usb.h"
#include "sys/char/tty.h"
#include "sys/sched.h"
#include "sys/panic.h"
#include "fs/sysfs.h"
#include "sys/init.h"
#include "net/net.h"
#include "fs/vfs.h"

#include "sys/debug.h"

void main(void) {
    char v = 0;
    for (int i = 0; i < 256; ++i) {
        ++v;
    }
    kinfo("???\n");

    pci_init();

    vfs_init();
    tty_init();

    sysfs_populate();

    syscall_init();
    sched_init();

    usb_daemon_start();
    net_daemon_start();
    user_init_start();

    sched_enter();

    panic("This code should not run\n");
}
