#pragma once
#include <stdint.h>

typedef struct {
    uint64_t rip;
    uint64_t cs;
    uint64_t rflags;
    uint64_t rsp;
    uint64_t ss;
} amd64_iret_regs_t;

typedef struct {
    uint64_t ds;
    uint64_t es;
    uint64_t fs;
    uint64_t gs;
} amd64_seg_regs_t;

typedef struct {
    uint64_t r15, r14, r13, r12, r11, r10, r9, r8;
    uint64_t rdi, rsi, rbp, rsp, rbx, rdx, rcx, rax;
} amd64_gp_regs_t;

typedef struct {
    amd64_seg_regs_t segs;
    uint64_t cr3;
    amd64_gp_regs_t gp;
    amd64_iret_regs_t iret;
} amd64_ctx_regs_t;

void amd64_ctx_dump(int level, const amd64_ctx_regs_t *regs);
