#include "arch/amd64/asm/asm_irq.h"
#include "arch/amd64/asm/asm_cpu.h"
.section .text
.global amd64_irq0
.global amd64_irq2_counting
.global amd64_irq0_early

amd64_irq0_early:
    cli
    swapgs_if_needed
    pushq %rax
    irq_eoi_lapic 0
    popq %rax
    swapgs_if_needed
    iretq

amd64_irq0:
    cli
    swapgs_if_needed

    // Push caller-saved registers so it appears as if a thread just called yield()
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

    call yield

    popq %rax
    popq %rdi
    popq %rsi
    popq %rdx
    popq %rcx
    popq %r8
    popq %r9
    popq %r10
    popq %r11

    swapgs_if_needed
    iretq

amd64_irq2_counting:
    cli
    incq get_cpu(0x10)
    push %rax
    irq_eoi_lapic 0
    pop %rax
    iretq
