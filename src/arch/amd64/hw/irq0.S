#include "arch/amd64/hw/int_macros.inc"

.section .text
.global amd64_irq_0
amd64_irq_0:
    cli

    __int_push_ctx

    call amd64_timer_tick

    // EOI
    mov $0x20, %dx
    mov $0x20, %al
    outb %al, %dx

    __int_pop_ctx

    iretq
