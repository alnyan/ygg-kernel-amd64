#include "sys/types.h"
#include "sys/panic.h"

struct amd64_exception_frame {
    uint64_t ss, rsp;
    uint64_t rflags;
    uint64_t cs, rip;

    uint64_t code, int_no;

    uint64_t rax, rcx, rdx, rbx;
    uint64_t rbp, rsi, rdi;
    uint64_t r8, r9, r10, r11;
    uint64_t r12, r13, r14, r15;

    uint64_t stack_canary;
};

void amd64_exception(struct amd64_exception_frame *frame) {

    panic("Exception without resolution\n");
}
