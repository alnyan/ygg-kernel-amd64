#include <config.h>
#include "arch/amd64/syscall.h"
#include "drivers/pci/pci.h"
#include "drivers/usb/usb.h"
#include "sys/char/tty.h"
#include "sys/console.h"
#include "sys/assert.h"
#include "sys/sched.h"
#include "sys/panic.h"
#include "fs/sysfs.h"
#include "sys/init.h"
#include "net/net.h"
#include "fs/vfs.h"

void _init(void) {
    extern char _init_start, _init_end;
    typedef void (*init_func_t) (void);

    init_func_t *init_array = (init_func_t *) &_init_start;

    // Sanity checks
    _assert(((uintptr_t) init_array & 0x7) == 0);
    _assert(((uintptr_t) (&_init_end) & 0x7) == 0);
    size_t count = ((uintptr_t) &_init_end - (uintptr_t) &_init_start) / sizeof(init_func_t);

    for (size_t i = 0; i < count; ++i) {
        _assert((((uintptr_t) init_array[i]) & 0xFFFFFF0000000000) == 0xFFFFFF0000000000);
        init_array[i]();
    }
}

void main(void) {
    pci_init();

    vfs_init();
    tty_init();

    sysfs_populate();

    syscall_init();
    sched_init();

#if defined(ENABLE_NET)
    net_init();
    net_daemon_start();
#endif
    usb_daemon_start();
    user_init_start();

    sched_enter();

    panic("This code should not run\n");
}
