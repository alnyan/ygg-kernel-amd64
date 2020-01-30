#include "sys/amd64/asm/asm_thread.h"
.section .bss
tmp0:
    .quad 0

.section .text
.global syscall_entry
syscall_entry:
    // %rcx - user rip
    // %rsp - user stack
    // %r11 - user rflags

    // Switch to kernel stack
    movq %rsp, tmp0(%rip)
    movq thread_current(%rip), %rsp
    movq THREAD_RSP0(%rsp), %rsp

    cmpq $123, %rax
    jz _syscall_fork

    pushq %rcx
    pushq %r11
    movq tmp0(%rip), %rcx
    pushq %rcx

    cmpq $256, %rax
    jg 1f
    leaq syscall_table(%rip), %rcx
    movq (%rcx, %rax, 8), %rcx
    test %rcx, %rcx
    jz 1f
    movq %rcx, %rax
    movq %r10, %rcx
    call *%rax
    jmp 2f
1:
    movq %rax, %rdi
    call syscall_undefined
2:

    popq %rdi
    popq %r11
    popq %rcx

    movq %rdi, %rsp

    sysretq

_syscall_fork:
    pushq %rcx
    pushq %r11
    movq tmp0(%rip), %rcx
    pushq %rcx

    pushq %r15
    pushq %r14
    pushq %r13
    pushq %r12
    pushq %rbp
    pushq %rbx

    pushq %r11
    pushq %r10
    pushq %r9
    pushq %r8
    pushq %rcx
    pushq %rdx
    pushq %rsi
    pushq %rdi

    movq %rsp, %rdi
    call sys_fork

    popq %rdi
    popq %rsi
    popq %rdx
    popq %rcx
    popq %r8
    popq %r9
    popq %r10
    popq %r11

    // Return to caller
    popq %rbx
    popq %rbp
    popq %r12
    popq %r13
    popq %r14
    popq %r15

    popq %rdi
    popq %r11
    popq %rcx

    movq %rdi, %rsp

    sysretq
