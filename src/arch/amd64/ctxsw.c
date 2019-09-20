#include "sys/debug.h"
#include "sys/thread.h"

// TODO: move to sched.c
thread_t *amd64_thread_current = NULL;
thread_t *amd64_thread_first = NULL;
thread_t *amd64_thread_last = NULL;

static thread_t threads[2];
static char kstacks[2 * 16384];

static void kthread0(void) {
    while (1) {
        kdebug("A\n");
    }
}

static void kthread1(void) {
    while (1) {
        kdebug("B\n");
    }
}

static void (*kthread_funcs[])(void) = {
    kthread0,
    kthread1
};

void amd64_ctx_add(thread_t *t) {
    if (!amd64_thread_first) {
        amd64_thread_last = t;
        amd64_thread_first = t;
    } else {
        amd64_thread_last->next = t;
        amd64_thread_last = t;
    }
}

void amd64_init_test_threads(void) {
    for (size_t i = 0; i < (sizeof(threads) / sizeof(threads[0])); ++i) {
        thread_info_init(&threads[i].info);

        threads[i].kstack_base = (uintptr_t) &kstacks[i * 16384];
        threads[i].kstack_size = 16384;
        threads[i].kstack_ptr = threads[i].kstack_base + threads[i].kstack_size - sizeof(amd64_thread_context_t);
        kdebug("base = %p, kstacks = %p, ptr = %p\n", threads[i].kstack_base,
                                                      kstacks,
                                                      threads[i].kstack_ptr);


        threads[i].info.flags = THREAD_KERNEL;

        thread_set_ip(&threads[i], (uintptr_t) kthread_funcs[i]);
        thread_set_space(&threads[i], mm_kernel);

        amd64_ctx_add(&threads[i]);
    }
}

int amd64_ctx_switch(void) {
    if (!amd64_thread_current) {
        amd64_thread_current = amd64_thread_first;
        if (!amd64_thread_current) {
            return -1;
        }
        return 0;
    }

    thread_t *from = amd64_thread_current;
    thread_t *to = from->next;

    if (to == NULL) {
        to = amd64_thread_first;
    }

    if (from == to) {
        // Switched to the same task - nothing happened
        return -1;
    }

    amd64_thread_current = to;

    return 0;
}
