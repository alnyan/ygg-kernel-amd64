#include "arch/amd64/asm/asm_irq.h"
.section .text
.align 16

.global amd64_irq_ipi
.global amd64_irq_ipi_panic

// Generic IPI handler
amd64_irq_ipi:
    cli

    irq_push_ctx

    call amd64_ipi_handle

    irq_eoi_lapic 0
    irq_pop_ctx

    iretq

// Kernel panic IPI handler
amd64_irq_ipi_panic:
    cli
    call amd64_ipi_panic
1:
    cli
    hlt
    jmp 1b
