#include "sys/amd64/hw/int_macros.inc"
.section .text
.extern panicf

amd64_irq_nyi_msg:
    .string "IRQ handler not implemented: %d\n"

.macro __irq_nyi_stub n
.global amd64_irq_\n
amd64_irq_\n:
    cli

    // Don't care about the stack now, won't return
    leaq amd64_irq_nyi_msg(%rip), %rdi
    movq $\n, %rsi
    xor %rax, %rax
    call panicf

    hlt
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
