#include "asm/asm_irq.h"
.section .text
.global amd64_irq0
.global amd64_irq0_counting
.extern amd64_timer_counter     // TODO: per-cpu

amd64_irq0:
    cli
    testq $1, amd64_irq0_counting(%rip)
    jnz amd64_irq0_count

    // Interrupt entry:
    // rsp should be one of the following:
    //  The one specified in the TSS.RSP0
    //  When interrupting kernel code - Task's kernel RSP
    irq_push_ctx

    // Let's adhere to the amd64 ABI
    movq %rsp, %rbp

    decq amd64_irq0_reload(%rip)
    jg 1f
    movq $10000, amd64_irq0_reload(%rip)
    irq_trace 0
1:

    // TODO: call sched here, do something

    // EOI
    irq_eoi_lapic 0

    irq_pop_ctx
    iretq

amd64_irq0_count:
    // Instead of running like a normal timer IRQ,
    // It'll just increment the counter so a better
    incq amd64_timer_counter(%rip)

    irq_eoi_lapic 0

    movb $0x20, %al
    outb %al, $0x20

    iretq

.section .data
amd64_irq0_counting:
    .quad 0
amd64_irq0_reload:
    .quad 10000
