#pragma once
#include "sys/types.h"

struct cpu_context;


extern uint64_t syscall_count;

void amd64_syscall_init(void);
__attribute__((noreturn)) void amd64_syscall_iretq(struct cpu_context *ctx);
__attribute__((noreturn)) void amd64_syscall_yield_stopped(void);
