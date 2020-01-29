#include "sys/amd64/asm/asm_irq.h"
#include "sys/amd64/asm/asm_cpu.h"

.section .text
.global amd64_exc_isr_14
amd64_exc_isr_14:
    cli
    hlt
    jmp amd64_exc_isr_14
