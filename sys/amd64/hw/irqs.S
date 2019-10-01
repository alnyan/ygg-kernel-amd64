#include "sys/amd64/hw/int_macros.inc"
.section .text
.extern panicf

amd64_irq_nyi_msg:
    .string "IRQ handler not implemented: %d\n"

.macro __irq_nyi_stub n
.global amd64_irq_\n
amd64_irq_\n:
    cli
    __int_push_ctx
    // Store current RSP
    movq sched_current(%rip), %rdi
    test %rdi, %rdi
    jz 1f
    movq %rsp, (%rdi)

    movq $\n, %rdi
    call amd64_irq_handler

    // Load next task's RSP
    movq sched_current(%rip), %rsi
    test %rsi, %rsi
    jz 1f
    movq (%rsi), %rsp

    movq %rsp, %rax
    addq $((8 + 8 + 5 + 5) * 8), %rax
    leaq amd64_tss(%rip), %rdi
    movq %rax, 4(%rdi)

1:
    __int_pop_ctx
    iretq
.endm

__irq_nyi_stub 1
__irq_nyi_stub 2
__irq_nyi_stub 3
__irq_nyi_stub 4
__irq_nyi_stub 5
__irq_nyi_stub 6
__irq_nyi_stub 7
__irq_nyi_stub 8
__irq_nyi_stub 9
__irq_nyi_stub 10
__irq_nyi_stub 11
__irq_nyi_stub 12
__irq_nyi_stub 13
__irq_nyi_stub 14
__irq_nyi_stub 15
