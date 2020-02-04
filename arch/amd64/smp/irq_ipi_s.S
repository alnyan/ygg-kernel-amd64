#include "arch/amd64/asm/asm_irq.h"
.section .text
.align 16

.global amd64_irq_ipi
.global amd64_irq_ipi_panic

// Generic IPI handler
amd64_irq_ipi:
    cli

    pushq %r11
    pushq %r10
    pushq %r9
    pushq %r8
    pushq %rcx
    pushq %rdx
    pushq %rsi
    pushq %rdi
    pushq %rax
    irq_eoi_lapic 0

    call amd64_ipi_handle

    popq %rax
    popq %rdi
    popq %rsi
    popq %rdx
    popq %rcx
    popq %r8
    popq %r9
    popq %r10
    popq %r11
    iretq

// Kernel panic IPI handler
amd64_irq_ipi_panic:
    cli
    call amd64_ipi_panic
1:
    cli
    hlt
    jmp 1b
