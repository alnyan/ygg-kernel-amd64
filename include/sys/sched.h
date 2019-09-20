/** vim: set ft=cpp.doxygen :
 * @file sys/sched.h
 * @brief Multitasking scheduler control and process management
 */
#pragma once
#include "sys/thread.h"

/**
 * @brief Add a thread to process queue
 * @param t Thread
 * @returns Assigned PID of the newly added thread on success,
 *          -1 on failure
 */
pid_t sched_add(thread_t *t);
