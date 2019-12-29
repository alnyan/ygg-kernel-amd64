#include "sys/amd64/cpu.h"
#include "sys/debug.h"

#if defined(AMD64_SMP)
struct cpu cpus[AMD64_MAX_SMP] = { 0 };
#else
struct cpu __amd64_cpu;
#endif

void cpu_print_context(int level, struct cpu_context *ctx) {
    kprint(level, "RAX = %p, RCX = %p\n", ctx->rax, ctx->rcx);
    kprint(level, "RDX = %p, RBX = %p\n", ctx->rdx, ctx->rbx);
    kprint(level, "RSP = %p, RBP = %p\n", ctx->rsp, ctx->rbp);
    kprint(level, "RSI = %p, RDI = %p\n", ctx->rsi, ctx->rdi);
    kprint(level, " R8 = %p,  R9 = %p\n", ctx->r8,  ctx->r9);
    kprint(level, "R10 = %p, R11 = %p\n", ctx->r10, ctx->r11);
    kprint(level, "R12 = %p, R13 = %p\n", ctx->r12, ctx->r13);
    kprint(level, "R14 = %p, R15 = %p\n", ctx->r12, ctx->r15);
}
