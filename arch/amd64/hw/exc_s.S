// vi: ft=asm :
#include "arch/amd64/asm/asm_irq.h"

.macro amd64_isr_nerr, n
amd64_exc_isr_\n:
    cli
    pushq $0
    pushq $\n
    jmp amd64_exc_generic
.endm

.macro amd64_isr_yerr, n
amd64_exc_isr_\n:
    cli
    pushq $\n
    jmp amd64_exc_generic
.endm

.section .text
amd64_exc_generic:
    // Push full CPU context for dump
    pushq %rax
    pushq %rcx
    pushq %rdx
    pushq %rbx
    // RSP already pushed
    pushq %rbp
    pushq %rsi
    pushq %rdi

    pushq %r8
    pushq %r9
    pushq %r10
    pushq %r11
    pushq %r12
    pushq %r13
    pushq %r14
    pushq %r15

    movq $AMD64_STACK_CTX_CANARY, %rax
    pushq %rax

    // amd64_exception(frame)
    movq %rsp, %rdi
    call amd64_exception

    movq $AMD64_STACK_CTX_CANARY, %rax
    test %rax, (%rsp)
    jz 1f
1:
    addq $8, %rsp

    popq %r15
    popq %r14
    popq %r13
    popq %r12
    popq %r11
    popq %r10
    popq %r9
    popq %r8

    popq %rdi
    popq %rsi
    popq %rbp

    popq %rbx
    popq %rdx
    popq %rcx
    popq %rax

    addq $16, %rsp

    iretq

amd64_isr_nerr 0
amd64_isr_nerr 1
amd64_isr_nerr 2
amd64_isr_nerr 3
amd64_isr_nerr 4
amd64_isr_nerr 5
amd64_isr_nerr 6
amd64_isr_nerr 7
amd64_isr_yerr 8
amd64_isr_nerr 9
amd64_isr_yerr 10
amd64_isr_yerr 11
amd64_isr_yerr 12
amd64_isr_yerr 13
amd64_isr_yerr 14
amd64_isr_nerr 15
amd64_isr_nerr 16
amd64_isr_yerr 17
amd64_isr_nerr 18
amd64_isr_nerr 19
amd64_isr_nerr 20
amd64_isr_nerr 21
amd64_isr_nerr 22
amd64_isr_nerr 23
amd64_isr_nerr 24
amd64_isr_nerr 25
amd64_isr_nerr 26
amd64_isr_nerr 27
amd64_isr_nerr 28
amd64_isr_nerr 29
amd64_isr_yerr 30
amd64_isr_nerr 31

.section .rodata
.global amd64_exception_vectors
amd64_exception_vectors:
    .quad amd64_exc_isr_0
    .quad amd64_exc_isr_1
    .quad amd64_exc_isr_2
    .quad amd64_exc_isr_3
    .quad amd64_exc_isr_4
    .quad amd64_exc_isr_5
    .quad amd64_exc_isr_6
    .quad amd64_exc_isr_7
    .quad amd64_exc_isr_8
    .quad amd64_exc_isr_9
    .quad amd64_exc_isr_10
    .quad amd64_exc_isr_11
    .quad amd64_exc_isr_12
    .quad amd64_exc_isr_13
    .quad amd64_exc_isr_14
    .quad amd64_exc_isr_15
    .quad amd64_exc_isr_16
    .quad amd64_exc_isr_17
    .quad amd64_exc_isr_18
    .quad amd64_exc_isr_19
    .quad amd64_exc_isr_20
    .quad amd64_exc_isr_21
    .quad amd64_exc_isr_22
    .quad amd64_exc_isr_23
    .quad amd64_exc_isr_24
    .quad amd64_exc_isr_25
    .quad amd64_exc_isr_26
    .quad amd64_exc_isr_27
    .quad amd64_exc_isr_28
    .quad amd64_exc_isr_29
    .quad amd64_exc_isr_30
    .quad amd64_exc_isr_31
