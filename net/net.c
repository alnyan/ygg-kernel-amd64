#include "sys/thread.h"
#include "sys/assert.h"
#include "sys/sched.h"
#include "sys/panic.h"
#include "sys/debug.h"

static struct thread netd_thread = {0};


static void net_daemon(void) {
    while (1) {
        asm volatile ("sti; hlt");
    }
}

int net_receive(/* TODO: replace with struct netdev * */ void *ctx, const void *data, size_t len) {
    kdebug("%u bytes\n", len);
    debug_dump(DEBUG_DEFAULT, data, len);
    return 0;
}

void net_daemon_start(void) {
    _assert(thread_init(&netd_thread, (uintptr_t) net_daemon, NULL, 0) == 0);
    netd_thread.pid = thread_alloc_pid(0);
    sched_queue(&netd_thread);
}
