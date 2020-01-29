#include "sys/amd64/asm/asm_irq.h"
#include "sys/amd64/asm/asm_cpu.h"
.section .text
.global amd64_irq0
.global amd64_irq2_counting
.global amd64_irq0_early

amd64_irq0_early:
    cli
    pushq %rax
    irq_eoi_lapic 0
    popq %rax
    iretq

_addr: .quad 0xFFFFFF00000B8000
_msg0: .string "CPU stopped!\n"

amd64_irq0:
    cli
    iretq

amd64_irq2_counting:
    cli
    incq get_cpu(0x10)
    push %rax
    irq_eoi_lapic 0
    pop %rax
    iretq

_fmt0:
    .string "RSP = %p\n"
