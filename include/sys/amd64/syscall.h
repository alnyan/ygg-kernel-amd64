#pragma once

struct cpu_context;

extern uint64_t syscall_count;

void amd64_syscall_init(void);
extern void amd64_syscall_iretq(struct cpu_context *ctx);
