#include "arch/amd64/regs.h"
#include "sys/debug.h"

void amd64_ctx_dump(int level, const amd64_ctx_regs_t *regs) {
    kprint(level, "rax = %p (%ld)\n", regs->gp.rax, regs->gp.rax);
    kprint(level, "rcx = %p (%ld)\n", regs->gp.rcx, regs->gp.rcx);
    kprint(level, "rdx = %p (%ld)\n", regs->gp.rdx, regs->gp.rdx);
    kprint(level, "rbx = %p (%ld)\n", regs->gp.rbx, regs->gp.rbx);

    kprint(level, "r8 = %p (%ld)\n", regs->gp.r8, regs->gp.r8);
    kprint(level, "r9 = %p (%ld)\n", regs->gp.r9, regs->gp.r9);
    kprint(level, "r10 = %p (%ld)\n", regs->gp.r10, regs->gp.r10);
    kprint(level, "r11 = %p (%ld)\n", regs->gp.r11, regs->gp.r11);
    kprint(level, "r12 = %p (%ld)\n", regs->gp.r12, regs->gp.r12);
    kprint(level, "r13 = %p (%ld)\n", regs->gp.r13, regs->gp.r13);
    kprint(level, "r14 = %p (%ld)\n", regs->gp.r14, regs->gp.r14);
    kprint(level, "r15 = %p (%ld)\n", regs->gp.r15, regs->gp.r15);

    if (regs->iret.cs == 0x08) {
        // Print stack pointer before the exception occurred
        kprint(level, "rsp = %p\n", regs->gp.rsp + sizeof(amd64_ctx_regs_t));
    }
    kprint(level, "rbp = %p\n", regs->gp.rbp);
    kprint(level, "rsi = %p\n", regs->gp.rsi);
    kprint(level, "rdi = %p\n", regs->gp.rdi);

    kprint(level, "pc: %02x:%p\n", regs->iret.cs, regs->iret.rip);

    if (regs->iret.cs != 0x08) {
        kprint(level, "stack: %02x:%p\n", regs->iret.ss, regs->iret.rsp);
    }
}
