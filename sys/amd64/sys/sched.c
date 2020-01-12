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
#include "sys/dev.h"
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
    struct vfs_ioctx ioctx = {0};
    struct stat st;
    int res;
    const char *root_dev_name = "ram0";
    struct vnode *root_dev;

    // Be [init] now
    thread_set_name(get_cpu()->thread, "init");

    // Create an alias for root device
    if ((res = dev_find(DEV_CLASS_BLOCK, root_dev_name, &root_dev)) != 0) {
        panic("Failed to find root device: %s\n", root_dev_name);
    }
    dev_add_link("root", root_dev);

    ext2_class_init();

    // Mount rootfs
    if ((res = vfs_mount(&ioctx, "/", root_dev->dev, "ustar", NULL)) != 0) {
        panic("Failed to mount root device: %s\n", kstrerror(res));
    }

    if ((res = vfs_mount(&ioctx, "/dev", NULL, "devfs", NULL)) != 0) {
        panic("Failed to mount devfs on /dev: %s\n", kstrerror(res));
    }

    struct thread *this = get_cpu()->thread;
    this->exit_code = 0;
    this->flags |= THREAD_STOPPED;
    amd64_syscall_yield_stopped();
}

static uint32_t sched_alloc_pid(int is_kernel) {
    // The first pid allocation will be 1 (init process)
    static uint32_t last_user_pid = 1;
    static uint32_t last_kernel_pid = 1;
    return is_kernel ? -(last_kernel_pid++) : last_user_pid++;
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

    t->pid = sched_alloc_pid(t->flags & THREAD_KERNEL);
    if (!(t->flags & THREAD_KERNEL)) {
        if (t->pid == 1) {
            t_user_init = t;
        }
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
                THREAD_INIT_CTX,
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
            THREAD_INIT_CTX,
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
    _assert(thr->pid == 1 || (thr->flags & THREAD_DONE_WAITING));

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

static void debug_print_thread_name(int level, struct thread *thr) {
    if (thr->flags & THREAD_KERNEL) {
        if (thr->name[0]) {
            debugf(level, "[%s] (%u)", thr->name, -thr->pid);
        } else {
            debugf(level, "K%u", -thr->pid);
        }
    } else {
        if (thr->name[0]) {
            debugf(level, "%s (%u)", thr->name, thr->pid);
        } else {
            debugf(level, "%u", thr->pid);
        }
    }
}

static void debug_thread_tree(struct thread *thr, size_t depth) {
    for (size_t i = 0; i < depth; ++i) {
        debugs(DEBUG_DEFAULT, "  ");
    }

    debug_print_thread_name(DEBUG_DEFAULT, thr);

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
            debug_print_thread_name(DEBUG_DEFAULT, it);

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
            kdebug("sigq = %x\n", to->sigq);
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
