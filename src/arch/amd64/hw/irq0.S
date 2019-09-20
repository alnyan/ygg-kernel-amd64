#include "arch/amd64/hw/int_macros.inc"

.section .text
.global amd64_irq_0
.extern amd64_tss
amd64_irq_0:
    cli

    __int_push_ctx

    // Store current RSP
    movq %rsp, %rax
    movq sched_current(%rip), %rdi
    test %rdi, %rdi
    jz 1f
    movq %rax, (%rdi)
1:

    movq amd64_timer_tick(%rip), %rax
    callq *%rax

    callq sched
    test %rax, %rax
    jnz __no_switch

    // Load next task's RSP
    movq sched_current(%rip), %rsi
    test %rsi, %rsi
    jz __no_switch
    movq (%rsi), %rsp

    movq %rsp, %rax
    addq $((8 + 8 + 5 + 5) * 8), %rax
    leaq amd64_tss(%rip), %rdi
    movq %rax, 4(%rdi)

    // TODO: TSS here
__no_switch:
    // EOI
    mov $0x20, %dx
    mov $0x20, %al
    outb %al, %dx

    __int_pop_ctx

    iretq
