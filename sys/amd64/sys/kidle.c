#include "sys/thread.h"
#include "sys/sched.h"
#include "sys/kidle.h"
#include "sys/debug.h"
#include "sys/time.h"

static char kidle_kstack[8192];
static thread_t kidle_thread;

static void kidle(void) {
    // Debug: print time ~each second
    uint32_t v_p = 0;
    while (1) {
        uint32_t v = systick / SYSTICK_RES;
        if (v != v_p) {
            kdebug("%d\n", v);
            v_p = v;
        }
        asm volatile ("sti; hlt");
    }
}

void kidle_init(void) {
    // TODO: functions for thread creation including kthread creation
    thread_info_init(&kidle_thread.info);

    kidle_thread.info.flags = THREAD_KERNEL;

    kidle_thread.kstack_base = (uintptr_t) kidle_kstack;
    kidle_thread.kstack_size = 4096;
    kidle_thread.kstack_ptr = kidle_thread.kstack_base + kidle_thread.kstack_size - sizeof(amd64_thread_context_t);

    amd64_thread_context_t *ctx = (amd64_thread_context_t *) kidle_thread.kstack_ptr;

    ctx->cr3 = (uintptr_t) mm_kernel - 0xFFFFFF0000000000;
    ctx->rip = (uintptr_t) kidle;
    ctx->cs = 0x08;
    ctx->ss = 0x10;
    ctx->rsp = kidle_thread.kstack_ptr;
    ctx->rflags = 0x248;

    kidle_thread.next = NULL;

    sched_add(&kidle_thread);
}
