/** vim:set ft=cpp.doxygen :
 * @file arch/amd64/sys/thread.h
 * @brief amd64 implementation of thread control structs/context storage
 */
#pragma once
#include "sys/thread.h"

#define thread_next(t)      ((t)->next)
#define thread_get(t)       (&(t)->info)

/**
 * @brief amd64 task kernel stack/context storage
 */
typedef struct {
    uint64_t seg[4];
    uint64_t cr3;
    uint64_t r[8];
    uint64_t rdi, rsi, rbp, __rsp;
    uint64_t rbx, rdx, rcx, rax;
    uint64_t rip, cs, rflags, rsp, ss;
}__attribute__((packed)) amd64_thread_context_t;

/**
 * @brief amd64-specific thread implementation
 */
struct thread {
    uintptr_t kstack_ptr;
    uintptr_t kstack_base;
    size_t kstack_size;

    thread_info_t info;

    thread_t *next;
};
