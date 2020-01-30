#include "sys/amd64/cpu.h"
#include "sys/syscall.h"
#include "sys/thread.h"
#include "sys/debug.h"
#include "sys/time.h"

#define MSR_IA32_STAR               0xC0000081
#define MSR_IA32_LSTAR              0xC0000082
#define MSR_IA32_SFMASK             0xC0000084

extern void syscall_entry(void);

void sys_debug_trace(const char *msg) {
    kdebug("Trace message: %s\n", msg);
}

void sys_debug_sleep(uint64_t ms) {
    thread_sleep(thread_self, system_time + ms * 1000000ULL);
}

void *syscall_table[256] = {
    NULL,

    [SYSCALL_NR_EXIT] = sys_exit,

    [SYSCALL_NR_DEBUG_SLEEP] = sys_debug_sleep,
    [SYSCALL_NR_DEBUG_TRACE] = sys_debug_trace,
};

void syscall_undefined(uint64_t rax) {
    kdebug("Undefined syscall: %d\n", rax);
}

void syscall_init(void) {
    // LSTAR = syscall_entry
    wrmsr(MSR_IA32_LSTAR, (uintptr_t) syscall_entry);

    // SFMASK = (1 << 9) /* IF */
    wrmsr(MSR_IA32_SFMASK, (1 << 9));

    // STAR = ((ss3 - 8) << 48) | (cs0 << 32)
    wrmsr(MSR_IA32_STAR, ((uint64_t) (0x1B - 8) << 48) | ((uint64_t) 0x08 << 32));

    // Set SCE bit
    uint64_t efer = rdmsr(MSR_IA32_EFER);
    efer |= (1 << 0);
    wrmsr(MSR_IA32_EFER, efer);
}
