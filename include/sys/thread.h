#pragma once

struct thread {
    // Platform data
    struct thread_data {
        uintptr_t rsp0;             // 0x00
        uintptr_t syscall_rsp;      // 0x08

        uintptr_t rsp0_top;         // 0x10
        uintptr_t cr3;              // 0x18

        uintptr_t rsp0_base;
        size_t    rsp0_size;

        uintptr_t rsp3_base;
        size_t    rsp3_size;
    } data;

    int pid;

    struct thread *prev, *next;
};
