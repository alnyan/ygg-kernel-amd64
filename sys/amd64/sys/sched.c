#include "sys/amd64/mm/mm.h"
#include "sys/amd64/cpu.h"
#include "sys/thread.h"
#include "sys/debug.h"
#include "sys/heap.h"
#include "sys/spin.h"
#include "sys/fs/vfs.h"
#include "sys/fs/fcntl.h"
#include "sys/errno.h"
#include "sys/blk.h"
#include "sys/panic.h"

static struct thread *sched_queue_heads[AMD64_MAX_SMP] = { 0 };
static struct thread *sched_queue_tails[AMD64_MAX_SMP] = { 0 };
static size_t sched_queue_sizes[AMD64_MAX_SMP] = { 0 };
static spin_t sched_lock = 0;

#define IDLE_STACK  32768
#define INIT_STACK  32768

// Testing
static char t_stack0[IDLE_STACK * AMD64_MAX_SMP] = {0};
static char t_stack1[INIT_STACK] = {0};
static struct thread t_idle[AMD64_MAX_SMP] = {0};
static struct thread t_init = {0};

void idle_func(uintptr_t id) {
    kdebug("Entering [idle] for cpu%d\n", id);
    while (1) {
        asm volatile ("sti; hlt");
    }
}

void init_func(void *arg) {
    kdebug("Entering [init]\n");

    // Mount rootfs if available
    struct blkdev *root_blk = blk_by_name("ram0");
    struct vfs_ioctx ioctx = {0};
    struct ofile fd;
    int res;

    if (root_blk) {
        if ((res = vfs_mount(&ioctx, "/", root_blk, "ustar", NULL)) != 0) {
            panic("mount rootfs: %s\n", kstrerror(res));
        }
    }

    while (1) {
        asm volatile ("sti; hlt");
    }
}

void sched_add_to(int cpu, struct thread *t) {
    t->next = NULL;

    spin_lock(&sched_lock);
    if (sched_queue_tails[cpu] != NULL) {
        sched_queue_tails[cpu]->next = t;
    } else {
        sched_queue_heads[cpu] = t;
    }
    sched_queue_tails[cpu] = t;
    ++sched_queue_sizes[cpu];
    spin_release(&sched_lock);
}

void sched_add(struct thread *t) {
    int min_i = -1;
    size_t min = 0xFFFFFFFF;
    spin_lock(&sched_lock);
    for (int i = 0; i < AMD64_MAX_SMP; ++i) {
        if (sched_queue_sizes[i] < min) {
            min = sched_queue_sizes[i];
            min_i = i;
        }
    }
    spin_release(&sched_lock);
    sched_add_to(min_i, t);
}

void sched_init(void) {
    for (int i = 0; i < AMD64_MAX_SMP; ++i) {
        thread_init(
                &t_idle[i],
                mm_kernel,
                (uintptr_t) idle_func,
                (uintptr_t) &t_stack0[IDLE_STACK * i],
                IDLE_STACK,
                0,
                0,
                THREAD_KERNEL,
                (void *) (uintptr_t) i);

        sched_add_to(i, &t_idle[i]);
    }

    // Also start [init] on cpu0
    thread_init(
            &t_init,
            mm_kernel,
            (uintptr_t) init_func,
            (uintptr_t) &t_stack1[INIT_STACK],
            INIT_STACK,
            0,
            0,
            THREAD_KERNEL,
            NULL);
    sched_add_to(0, &t_init);
}

int sched(void) {
    struct thread *from = get_cpu()->thread;
    struct thread *to;

    spin_lock(&sched_lock);
    if (from && from->next) {
        to = from->next;
    } else {
        to = sched_queue_heads[get_cpu()->processor_id];
    }
    spin_release(&sched_lock);

    // Empty scheduler queue
    if (!to) {
        return -1;
    }

    get_cpu()->thread = to;

    return 0;
}
