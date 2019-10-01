#include "sys/thread.h"
#include "sys/panic.h"
#include "sys/assert.h"
#include "sys/debug.h"
#include "sys/heap.h"

#define AMD64_DEFAULT_KSTACK_SIZE   0x4000

#define ctx0(t)    ((amd64_thread_context_t *) (t->kstack_ptr))

int thread_init(thread_t *t,
                mm_space_t space,
                uintptr_t ip,
                uintptr_t kstack_base,
                size_t kstack_size,
                uintptr_t ustack_base,
                size_t ustack_size,
                uint32_t flags
        ) {
    if (thread_info_init(&t->info) != 0) {
        return -1;
    }
    t->info.flags = flags;

    // Mandatory for both kernel and user threads
    if (!kstack_base) {
        kstack_base = (uintptr_t) kmalloc(AMD64_DEFAULT_KSTACK_SIZE);
        kstack_size = AMD64_DEFAULT_KSTACK_SIZE;
        if (!kstack_base) {
            panic("Failed to allocate thread kstack\n");
        }
    }
    t->kstack_base = kstack_base;
    t->kstack_size = kstack_size;
    t->kstack_ptr = t->kstack_base + t->kstack_size - sizeof(amd64_thread_context_t);

    // If we're not kernel, additionally set the ustack
    if (!(flags & THREAD_KERNEL)) {
        assert(ustack_base, "Tried to create an userspace task, but passed no ustack\n");

        panic("Not implemented\n");
    }

    // Set CS:RIP
    if (flags & THREAD_KERNEL) {
        ctx0(t)->cs = 0x08;
        ctx0(t)->ss = 0x10;
        ctx0(t)->rsp = t->kstack_ptr;
        ctx0(t)->rflags = 0x248;
        ctx0(t)->rip = ip;
    } else {
        panic("Not implemented\n");
    }

    assert(((uintptr_t) space) > 0xFFFFFF0000000000,
           "Invalid PML4 address provided: %p\n", space);

    // Set task space
    t->info.space = space;
    ctx0(t)->cr3 = ((uintptr_t) space) - 0xFFFFFF0000000000;

    t->next = NULL;

    return 0;
}

void thread_set_ip(thread_t *t, uintptr_t ip) {
    if (t->info.flags & THREAD_KERNEL) {
        ctx0(t)->cs = 0x08;
        ctx0(t)->ss = 0x10;
        ctx0(t)->rsp = t->kstack_ptr;
        ctx0(t)->rflags = 0x248;
        ctx0(t)->rip = ip;
    } else {
        panic("Userspace threads are not supported yet\n");
    }
}

void thread_set_space(thread_t *t, mm_space_t pd) {
    assert(((uintptr_t) pd) > 0xFFFFFF0000000000, "Invalid PML4 address provided: %p\n", pd);

    t->info.space = pd;
    ctx0(t)->cr3 = ((uintptr_t) pd) - 0xFFFFFF0000000000;
}

void thread_set_ustack(thread_t *t, uintptr_t base, size_t size) {
    panic("NYI\n");
}