#include "sys/types.h"
#include "sys/debug.h"
#include "sys/panic.h"

#define X86_FLAGS_CF            (1 << 0)
#define X86_FLAGS_PF            (1 << 2)
#define X86_FLAGS_AF            (1 << 4)
#define X86_FLAGS_ZF            (1 << 6)
#define X86_FLAGS_SF            (1 << 7)
#define X86_FLAGS_TF            (1 << 8)
#define X86_FLAGS_IF            (1 << 9)
#define X86_FLAGS_DF            (1 << 10)
#define X86_FLAGS_OF            (1 << 11)

struct amd64_exception_frame {
    uint64_t stack_canary;

    uint64_t r15, r14, r13, r12;
    uint64_t r11, r10, r9, r8;

    uint64_t rdi, rsi, rbp;
    uint64_t rbx, rdx, rcx, rax;

    uint64_t exc_no, exc_code;

    uint64_t rip, cs, rflags, rsp, ss;
};

void amd64_exception(struct amd64_exception_frame *frame) {
    // Dump frame
    kfatal("CPU raised exception #%u\n", frame->exc_no);

    kfatal("%%rax = %p, %%rcx = %p\n", frame->rax, frame->rcx);
    kfatal("%%rdx = %p, %%rbx = %p\n", frame->rdx, frame->rbx);
    kfatal("%%rbp = %p\n", frame->rbp);
    kfatal("%%rsi = %p, %%rdi = %p\n", frame->rsi, frame->rdi);

    kfatal("%%r8  = %p, %%r9  = %p\n",  frame->r8,  frame->r9);
    kfatal("%%r10 = %p, %%r11 = %p\n", frame->r10, frame->r11);
    kfatal("%%r12 = %p, %%r13 = %p\n", frame->r12, frame->r13);
    kfatal("%%r14 = %p, %%r15 = %p\n", frame->r14, frame->r15);

    kfatal("Execution context:\n");
    kfatal("%%cs:%%rip = %02x:%p\n", frame->cs, frame->rip);
    kfatal("%%ss:%%rsp = %02x:%p\n", frame->ss, frame->rsp);
    kfatal("%%rflags = %08x (IOPL=%u %c%c%c%c%c%c%c %c%c)\n",
            frame->rflags,
            (frame->rflags >> 12) & 0x3,
            (frame->rflags & X86_FLAGS_CF) ? 'C' : '-',
            (frame->rflags & X86_FLAGS_PF) ? 'P' : '-',
            (frame->rflags & X86_FLAGS_AF) ? 'A' : '-',
            (frame->rflags & X86_FLAGS_ZF) ? 'Z' : '-',
            (frame->rflags & X86_FLAGS_SF) ? 'S' : '-',
            (frame->rflags & X86_FLAGS_DF) ? 'D' : '-',
            (frame->rflags & X86_FLAGS_OF) ? 'O' : '-',
            (frame->rflags & X86_FLAGS_IF) ? 'I' : '-',
            (frame->rflags & X86_FLAGS_TF) ? 'T' : '-');

    panic("Exception without resolution\n");
}
