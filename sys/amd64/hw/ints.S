#include "sys/amd64/hw/int_macros.inc"
.section .text
.extern amd64_err_num
// For legacy system call interface (via int $0x80)
.global amd64_isr_syscall

__isr_generic:
    cli

    // TODO: preserve error codes
    pushq %rax
    mov 8(%rsp), %rax
    mov %rax, amd64_err_num(%rip)
    mov 16(%rsp), %rax
    mov %rax, amd64_err_code(%rip)
    popq %rax
    add $16, %rsp

    __int_push_ctx

    movq %rsp, %rdi
    call amd64_exc_handler

    // TODO: allow handling and returning from an exception ISR
1:
    hlt
    jmp 1b

// ISR with error code
.macro __isr_yerr n
.global amd64_isr_\n
amd64_isr_\n:
    pushq $\n
    jmp __isr_generic
.endm

// ISR with no error code
.macro __isr_nerr n
.global amd64_isr_\n
amd64_isr_\n:
    pushq $0
    pushq $\n
    jmp __isr_generic
.endm

__isr_nerr 0
__isr_nerr 1
__isr_nerr 2
__isr_nerr 3
__isr_nerr 4
__isr_nerr 5
__isr_nerr 6
__isr_nerr 7
__isr_yerr 8
__isr_nerr 9
__isr_yerr 10
__isr_yerr 11
__isr_yerr 12
__isr_yerr 13
__isr_yerr 14
__isr_nerr 15
__isr_nerr 16
__isr_yerr 17
__isr_nerr 18
__isr_nerr 19
__isr_nerr 20
__isr_nerr 21
__isr_nerr 22
__isr_nerr 23
__isr_nerr 24
__isr_nerr 25
__isr_nerr 26
__isr_nerr 27
__isr_nerr 28
__isr_nerr 29
__isr_yerr 30
__isr_nerr 31
