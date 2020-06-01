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

void main(void) {
    pci_init();

    vfs_init();
    tty_init();

    sysfs_populate();

    syscall_init();
    sched_init();

    net_init();
    usb_daemon_start();
    net_daemon_start();
    user_init_start();

    sched_enter();

    panic("This code should not run\n");
}
