#include "sys/amd64/mm/mm.h"
#include "sys/amd64/mm/pool.h"
#include "sys/amd64/syscall.h"
#include "sys/amd64/cpu.h"
#include "sys/net/eth.h"
#include "sys/signum.h"
#include "sys/thread.h"
#include "sys/reboot.h"
#include "sys/debug.h"
#include "sys/heap.h"
#include "sys/spin.h"
#include "sys/fs/ext2.h"
#include "sys/fs/vfs.h"
#include "sys/fcntl.h"
#include "sys/errno.h"
#include "sys/blk.h"
#include "sys/time.h"
#include "sys/panic.h"
#include "sys/assert.h"
#include "sys/string.h"
#include "sys/binfmt_elf.h"
#include "sys/tty.h"
#include "sys/mm.h"

void sched_add_to(int cpu, struct thread *t);
void sched_add(struct thread *t);

static struct thread *sched_queue_heads[AMD64_MAX_SMP] = { 0 };
static struct thread *sched_queue_tails[AMD64_MAX_SMP] = { 0 };
static size_t sched_queue_sizes[AMD64_MAX_SMP] = { 0 };
static size_t sched_ncpus = 1;
static spin_t sched_lock = 0;

// Non-zero means we're terminating all the stuff and performing an action
// once we're done
static unsigned int system_state = 0;

#define IDLE_STACK  4096
#define INIT_STACK  32768

// Testing
static char t_stack0[IDLE_STACK * AMD64_MAX_SMP] = {0};
static char t_stack1[INIT_STACK] = {0};
static struct thread t_idle[AMD64_MAX_SMP] = {0};
static struct thread t_init = {0};
static struct thread *t_user_init = NULL;

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
    ext2_class_init();

    if (root_blk) {
        if ((res = vfs_mount(&ioctx, "/", root_blk, "ustar", NULL)) != 0) {
            panic("mount rootfs: %s\n", kstrerror(res));
        }

        // Mount devfs
        if ((res = vfs_mount(&ioctx, "/dev", NULL, "devfs", NULL)) != 0) {
            panic("mount devfs: %s\n", kstrerror(res));
        }

        // Try to mount /dev/sda1 at /mnt (don't panic on failure, though)
        struct blkdev *sda1_blk = blk_by_name("sda1");
        if (sda1_blk) {
            kinfo("Trying to mount /dev/sda1 -> /mnt\n");

            if ((res = vfs_mount(&ioctx, "/mnt", sda1_blk, "auto", NULL)) != 0) {
                kwarn("/dev/sda1: %s\n", kstrerror(res));
            }
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

        // Free memory
        kfree(exec_buf);

        // FIXME: this way of opening requires devfs to be mounted at /dev, I guess
        //        I can just have dev_entry have a vnode for each of them (and remove
        //        ones used in devfs mapper)
        // Set tty0 input
        if ((res = vfs_open(&init_thread->ioctx, &init_thread->fds[0], "/dev/tty0", O_RDONLY, 0)) < 0) {
            panic("Failed to set up tty0 for input: %s\n", kstrerror(res));
        }
        if ((res = vfs_open(&init_thread->ioctx, &init_thread->fds[1], "/dev/tty0", O_WRONLY, 0)) < 0) {
            panic("Failed to set up tty0 for output: %s\n", kstrerror(res));
        }

        kdebug("Done\n");
        sched_add(init_thread);
    }

    while (1) {
        asm volatile ("sti; hlt");

        // I've decided not to create a separate thread for network handling yet,
        // so the code will be here
        ethq_handle();
    }
}

static uint32_t sched_alloc_pid(void) {
    // The first pid allocation will be 1 (init process)
    static uint32_t last_pid = 1;
    return last_pid++;
}

#if defined(AMD64_SMP)
void sched_set_cpu_count(size_t count) {
    sched_ncpus = count;
}
#endif

struct thread *sched_find(int pid) {
    if (pid <= 0) {
        return NULL;
    }

    // Search through queues
    for (size_t cpu = 0; cpu < sched_ncpus; ++cpu) {
        for (struct thread *t = sched_queue_heads[cpu]; t; t = t->next) {
            if ((int) t->pid == pid) {
                return t;
            }
        }
    }

    return NULL;
}

void sched_add_to(int cpu, struct thread *t) {
    t->next = NULL;

    spin_lock(&sched_lock);
    if (sched_queue_tails[cpu] != NULL) {
        sched_queue_tails[cpu]->next = t;
    } else {
        sched_queue_heads[cpu] = t;
    }
    t->prev = sched_queue_tails[cpu];
    t->cpu = cpu;

    if (!(t->flags & THREAD_KERNEL)) {
        t->pid = sched_alloc_pid();
        if (t->pid == 1) {
            t_user_init = t;
        }
    } else {
        t->pid = 0;
    }
    sched_queue_tails[cpu] = t;
    ++sched_queue_sizes[cpu];
    spin_release(&sched_lock);
}

void sched_add(struct thread *t) {
    int min_i = -1;
    size_t min = 0xFFFFFFFF;
    spin_lock(&sched_lock);
    for (size_t i = 0; i < sched_ncpus; ++i) {
        if (sched_queue_sizes[i] < min) {
            min = sched_queue_sizes[i];
            min_i = i;
        }
    }
    spin_release(&sched_lock);
    sched_add_to(min_i, t);
}

void sched_init(void) {
    for (size_t i = 0; i < sched_ncpus; ++i) {
        thread_init(
                &t_idle[i],
                mm_kernel,
                (uintptr_t) idle_func,
                (uintptr_t) &t_stack0[i * IDLE_STACK],
                IDLE_STACK,
                0,
                0,
                THREAD_KERNEL,
                (void *) i);
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

static void thread_unchild(struct thread *thr) {
    struct thread *par = thr->parent;
    if (par) {
        kdebug("Unchilding %d from %d\n", thr->pid, par->pid);
        struct thread *p = NULL;
        struct thread *c = par->child;
        int found = 0;

        while (c) {
            if (c == thr) {
                found = 1;
                if (p) {
                    p->next_child = thr->next_child;
                } else {
                    par->child = thr->next_child;
                }
                break;
            }

            p = c;
            c = c->next_child;
        }

        _assert(found);
        if ((par->flags & THREAD_ZOMBIE) && !par->child) {
            thread_unchild(par);

            // Zombie is finally ready to die
            kdebug("Thread cleanup: zombie %d\n", par->pid);
            thread_cleanup(par);
        }
    }
}

// Called after parent is done waiting
void thread_terminate(struct thread *thr) {
    _assert(thr->flags & THREAD_STOPPED);
    _assert(thr->flags & THREAD_DONE_WAITING);

    if (!thr->child) {
        // Remove from parent's "child" list
        thread_unchild(thr);

        kdebug("Thread cleanup: %d\n", thr->pid);
        thread_cleanup(thr);
    } else {
        // TODO: add to some kind of "zombie" queue
        //       mark children as "zombie-children" to notify parent task
        kdebug("Couldn't terminate thread %d: has children\n", thr->pid);
        thr->flags |= THREAD_ZOMBIE;
    }
}

void sched_remove_from(int cpu, struct thread *thr) {
    struct thread *prev = thr->prev;
    struct thread *next = thr->next;
    kdebug("cpu%d: removing thread %d\n", cpu, thr->pid);

    if (prev) {
        prev->next = next;
    } else {
        sched_queue_heads[cpu] = next;
    }

    if (next) {
        next->prev = prev;
    } else {
        sched_queue_tails[cpu] = prev;
    }

    --sched_queue_sizes[cpu];

    thr->next = NULL;
    thr->prev = NULL;

    // TODO: child process sends SIGCHLD to its parent (ignored by default)
    if (thr->pid == 1) {
        thread_terminate(thr);
    }
}

void sched_remove(struct thread *thr) {
    kdebug("Remove %p\n", thr);
    int should_panic = (thr->pid == 1) && !system_state;
    if (thr->pid == 1) {
        t_user_init = NULL;
    }
    sched_remove_from(thr->cpu, thr);

    if (should_panic) {
        panic("PID 1 got killed\n");
    }
}

void sched_reboot(unsigned int cmd) {
    if (system_state) {
        panic("sched_reboot requested when system_state is already non-zero\n");
    }
    _assert(t_user_init);

    system_state = cmd;
    // Send SIGTERM to init
    t_user_init->sigq |= 1 << (SIGTERM - 1);
}

static uint64_t debug_last_tick = 0;

static void debug_thread_tree(struct thread *thr, size_t depth) {
    for (size_t i = 0; i < depth; ++i) {
        debugs(DEBUG_DEFAULT, "  ");
    }
    debugf(DEBUG_DEFAULT, "Thread %d", thr->pid);

    if (thr->flags & THREAD_ZOMBIE) {
        debugc(DEBUG_DEFAULT, 'z');
    }
    if (thr->flags & THREAD_STOPPED) {
        debugc(DEBUG_DEFAULT, 's');
    }

    if (thr->child) {
        debugs(DEBUG_DEFAULT, " {\n");
        for (struct thread *ch = thr->child; ch; ch = ch->next_child) {
            debug_thread_tree(ch, depth + 1);
        }
        for (size_t i = 0; i < depth; ++i) {
            debugs(DEBUG_DEFAULT, "  ");
        }
        debugc(DEBUG_DEFAULT, '}');
    }
    debugc(DEBUG_DEFAULT, '\n');
}

static void debug_stats(void) {
    kdebug("--- STATS ---\n");
    kdebug("syscalls/s: %lu\n", syscall_count);

    if (t_user_init) {
        kdebug("init [1] tree:\n");
        debug_thread_tree(t_user_init, 0);
    }

    kdebug("CPU queues:\n");
    for (size_t i = 0; i < sched_ncpus; ++i) {
        debugf(DEBUG_DEFAULT, "  cpu%d [ ", i);
        for (struct thread *it = sched_queue_heads[i]; it; it = it->next) {
            if (it->flags & THREAD_KERNEL) {
                // Kernel threads have no pids, lol
                debugs(DEBUG_DEFAULT, "K?");
            } else {
                debugf(DEBUG_DEFAULT, "%d", it->pid);
            }

            if (it->next) {
                debugs(DEBUG_DEFAULT, ", ");
            }
        }
        debugs(DEBUG_DEFAULT, " ]\n");
    }

    kdebug("--- ----- ---\n");

    syscall_count = 0;
}

int sched(void) {
    struct thread *from = get_cpu()->thread;
    struct thread *to;
    int cpu = get_cpu()->processor_id;
    uintptr_t flags;

    // Print various stuff every second
    if (!cpu && system_time - debug_last_tick >= 1000000000) {
        debug_stats();
        debug_last_tick = system_time;
    }

    if (from == &t_idle[cpu]) {
        from = NULL;
    }

    if (system_state && !t_user_init) {
        // All user tasks are terminated
        system_power_cmd(system_state);
    }

    if (from && from->next) {
        to = from->next;
    } else {
        to = sched_queue_heads[cpu];
    }

    if (to && to->flags & THREAD_STOPPED) {
        to = NULL;
    }

    // Empty scheduler queue
    if (!to) {
        to = &t_idle[cpu];
        get_cpu()->thread = to;
        return 0;
    }

    if (to->flags & THREAD_SIGRET) {
        to->flags &= ~THREAD_SIGRET;
        amd64_thread_sigret(to);
    }

    if (to->sigq) {
        // Should we enter signal handler?
        int signum = 0;
        if (to->sigq & (1 << 8)) {
            // SIGKILL
            panic("TODO: properly handle SIGKILL\n");
        }

        for (size_t i = 0; i < 32; ++i) {
            if (to->sigq & (1 << i)) {
                signum = i + 1;
                to->sigq &= ~(1 << i);
                break;
            }
        }
        _assert(signum);

        amd64_thread_sigenter(to, signum);
    }

    get_cpu()->thread = to;

    return 0;
}
