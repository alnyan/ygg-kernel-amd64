#include "sys/amd64/mm/mm.h"
#include "sys/amd64/mm/pool.h"
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
#include "sys/assert.h"
#include "sys/string.h"
#include "sys/binfmt_elf.h"
#include "sys/tty.h"
#include "sys/mm.h"

void sched_add_to(int cpu, struct thread *t);

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
    struct stat st;
    int res;

    if (root_blk) {
        if ((res = vfs_mount(&ioctx, "/", root_blk, "ustar", NULL)) != 0) {
            panic("mount rootfs: %s\n", kstrerror(res));
        }

        if ((res = vfs_stat(&ioctx, "/init", &st)) != 0) {
            panic("/init: %s\n", kstrerror(res));
        }

        if ((st.st_mode & S_IFMT) != S_IFREG) {
            panic("/init: not a regular file\n");
        }

        if (!(st.st_mode & 0111)) {
            panic("/init: not executable\n");
        }

        kdebug("/init is %S\n", st.st_size);

        void *exec_buf = kmalloc(st.st_size);
        size_t pos = 0;
        ssize_t bread;
        assert(exec_buf, "Failed to allocate buffer for reading\n");

        if ((res = vfs_open(&ioctx, &fd, "/init", 0, O_RDONLY)) != 0) {
            panic("open(): %s\n", kstrerror(res));
        }

        while ((bread = vfs_read(&ioctx, &fd, (char *) exec_buf + pos, MIN(512, st.st_size - pos))) > 0) {
            pos += bread;
        }

        vfs_close(&ioctx, &fd);

        kdebug("Successfully read init\n");

        struct thread *init_thread = (struct thread *) kmalloc(sizeof(struct thread));
        _assert(init_thread);
        mm_space_t thread_space = amd64_mm_pool_alloc();
        _assert(thread_space);
        mm_space_clone(thread_space, mm_kernel, MM_CLONE_FLG_KERNEL);

        if (thread_init(
                    init_thread,
                    thread_space,
                    0,
                    0,
                    0,
                    0,
                    0,
                    0,
                    NULL) < 0) {
            panic("Failed to setup init task\n");
        }

        if ((res = elf_load(init_thread, exec_buf)) < 0) {
            panic("Failed to load binary: %s\n", kstrerror(res));
        }

        // Set tty0 input
        init_thread->fds[0].flags = O_RDONLY;
        init_thread->fds[0].vnode = tty0;
        init_thread->fds[0].pos = 0;

        init_thread->fds[1].flags = O_WRONLY;
        init_thread->fds[1].vnode = tty0;
        init_thread->fds[1].pos = 0;

        kdebug("Done\n");
        sched_add_to(0, init_thread);
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
