#include "sys/debug.h"
#include "sys/thread.h"

#define KSTACK_SIZE 32768

// TODO: move to sched.c
thread_t *amd64_thread_current = NULL;
thread_t *amd64_thread_first = NULL;
thread_t *amd64_thread_last = NULL;

static thread_t threads[3];
static char kstacks[3 * KSTACK_SIZE];
static char ustacks[3 * 32768];

static void kthread0(void) {
    while (1) {
        asm volatile ("syscall");
        //kdebug("A\n");
        //for (int i = 0; i < 10000000; ++i);
    }
}

static void kthread1(void) {
    //kdebug("Start\n");
    while (1) {
        asm volatile ("syscall");
        //kdebug("B\n");
        //for (int i = 0; i < 10000000; ++i);
        //kdebug("C\n");
        //for (int i = 0; i < 10000000; ++i);
    }
}

static void kthread2(void) {
    while (1) {
        for (int i = 0; i < 10000000; ++i);
        kdebug("Kthread\n");
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
    amd64_thread_current = NULL;
    amd64_thread_context_t *ctx;

    for (size_t i = 0; i < 2; ++i) {
        thread_info_init(&threads[i].info);

        threads[i].kstack_base = (uintptr_t) &kstacks[i * KSTACK_SIZE];
        threads[i].kstack_size = KSTACK_SIZE;
        threads[i].kstack_ptr = threads[i].kstack_base + threads[i].kstack_size - sizeof(amd64_thread_context_t);
        ctx = (amd64_thread_context_t *) threads[i].kstack_ptr;

        //threads[i].info.flags = THREAD_KERNEL;
        ctx->cr3 = (uintptr_t) mm_kernel - 0xFFFFFF0000000000;
        ctx->rip = (uintptr_t) kthread_funcs[i];
        ctx->cs = 0x23;
        ctx->rsp = (uintptr_t) &ustacks[(i + 1) * 32768];
        ctx->ss = 0x1B;

        ctx->rflags = 0x248;

        threads[i].next = NULL;

        //thread_set_ip(&threads[i], (uintptr_t) kthread_funcs[i]);
        //thread_set_space(&threads[i], mm_kernel);

        amd64_ctx_add(&threads[i]);
    }

    thread_info_init(&threads[2].info);

    threads[2].kstack_base = (uintptr_t) &kstacks[2 * KSTACK_SIZE];
    threads[2].kstack_size = KSTACK_SIZE;
    threads[2].kstack_ptr = threads[2].kstack_base + threads[2].kstack_size - sizeof(amd64_thread_context_t);
    ctx = (amd64_thread_context_t *) threads[2].kstack_ptr;

    //threads[i].info.flags = THREAD_KERNEL;
    ctx->cr3 = (uintptr_t) mm_kernel - 0xFFFFFF0000000000;
    ctx->rip = (uintptr_t) kthread2;
    ctx->cs = 0x08;
    ctx->rsp = threads[2].kstack_ptr;
    ctx->ss = 0x10;

    ctx->rflags = 0x248;

    threads[2].next = NULL;

    //thread_set_ip(&threads[i], (uintptr_t) kthread_funcs[i]);
    //thread_set_space(&threads[i], mm_kernel);

    amd64_ctx_add(&threads[2]);
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
