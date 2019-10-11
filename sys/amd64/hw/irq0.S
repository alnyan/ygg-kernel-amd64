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

    // Store old ctx
    movq get_cpu(0x08), %rdi
    test %rdi, %rdi
    jz 1f
    movq %rsp, (%rdi)
1:

    call sched
    // Non-zero return code means no sched happened
    test %rax, %rax
    jnz 1f

    // rsi = thread structure pointer
    movq get_cpu(0x08), %rsi
    movq 0(%rsi), %rsp
1:
    // Assume we're now on TASK2's stack

    // EOI
    irq_eoi_lapic 0

    irq_pop_ctx

    swapgs
    iretq

_fmt0:
    .string "RSP = %p\n"
