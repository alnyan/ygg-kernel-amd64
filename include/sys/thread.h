/** vim: set ft=cpp.doxygen :
 * @file sys/thread.h
 * @brief Multithreading and thread context manipulation functions
 * @note For now, let's define `thread' the same as `process'
 */
#pragma once
#include "sys/types.h"
#include "sys/mm.h"

#define THREAD_KERNEL   (1 << 0)

/**
 * @brief Process ID type (Thread ID, too)
 */
typedef int32_t pid_t;

/**
 * @brief Opaque type for thread, details are platform-specific
 */
typedef struct thread thread_t;

/**
 * @brief Generic thread information
 * @note Platform-specific implementations provide thread_get(t) macro
 *       to extract this.
 */
typedef struct thread_info {
    pid_t pid;
    pid_t parent;
    uint32_t flags;
    uint32_t status;
    mm_space_t space;
} thread_info_t;

#if defined(ARCH_AMD64)
#include "sys/amd64/sys/thread.h"
#endif

////////

/**
 * @brief Current thread context
 */
extern thread_t *sched_current;

/**
 * @brief Initialize thread information struct
 * @param info Thread info struct
 */
int thread_info_init(thread_info_t *info);

////////

/**
 * @brief Setup thread data structures
 * @param t Thread
 */
int thread_init(thread_t *t);

/**
 * @brief Set thread instruction pointer
 * @param t Thread
 * @param ip Address of entry point
 * @note In order for this function to handle kernel threads properly,
 *       set THREAD_KERNEL flag @b before calling this one.
 */
void thread_set_ip(thread_t *t, uintptr_t ip);

/**
 * @brief Set thread memory space
 * @param t Thread
 * @param pd Virtual memory space
 */
void thread_set_space(thread_t *t, mm_space_t pd);

/**
 * @brief Set thread userspace stack
 * @param t Thread
 * @param bottom Start address of the stack
 * @param size Size of the user stack in bytes
 * @warning Will panic if thread is kernel-space
 */
void thread_set_ustack(thread_t *t, uintptr_t bottom, size_t size);
