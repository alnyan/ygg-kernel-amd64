#include "sys/thread.h"
#include "sys/debug.h"
#include "sys/syscall.h"

void amd64_syscall(uint64_t a0, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t no) {
    switch (no) {
    case SYS_NR_WRITE:
        // TODO: vfs_write
        // XXX: this pointer is userspace, but we still can access it, as CR3
        //      still points to userspace PML4
        kdebug("%p %p %u\n", a0, a1, a2);
        for (size_t i = 0; i < a2; ++i) {
            debugc(DEBUG_INFO, ((const char *) a1)[i]);
        }
        return;
    default:
        return;
    }
}
