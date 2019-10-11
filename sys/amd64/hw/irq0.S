#include "asm/asm_irq.h"
.section .text
.global amd64_irq0

amd64_irq0:
    cli
    // Interrupt entry:
    // rsp should be one of the following:
    //  The one specified in the TSS.RSP0
    //  When interrupting kernel code - Task's kernel RSP
    irq_push_ctx

    // Let's adhere to the amd64 ABI
    movq %rsp, %rbp

    irq_trace 0

    // TODO: call sched here, do something

    // EOI
    irq_eoi_lapic 0

    irq_pop_ctx
    iretq
