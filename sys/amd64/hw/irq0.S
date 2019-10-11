#include "sys/amd64/asm/asm_irq.h"
#include "sys/amd64/asm/asm_cpu.h"
.section .text
.global amd64_irq0

amd64_irq0:
    cli
    swapgs

    // Interrupt entry:
    // rsp should be one of the following:
    //  The one specified in the TSS.RSP0
    //  When interrupting kernel code - Task's kernel RSP
    irq_push_ctx

    // Let's adhere to the amd64 ABI
    movq %rsp, %rbp

    // Increment tick counter
    incq get_cpu(0x10)
    testq $(1 << 5), get_cpu(0x10)
    jz 1f
    movq $0, get_cpu(0x10)
    irq_trace 0
1:

    // TODO: call sched here, do something

    // EOI
    irq_eoi_lapic 0

    irq_pop_ctx

    swapgs
    iretq
