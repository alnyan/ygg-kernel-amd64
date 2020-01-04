#include "sys/amd64/asm/asm_irq.h"
#include "sys/amd64/asm/asm_cpu.h"
.section .text
.global amd64_irq0
.global amd64_irq2_counting

amd64_irq0:
    cli
    // Stack:
    // ss
    // rsp
    // rflags
    // cs
    // rip
    swapgs_if_needed

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

    // Write TSS entry
    movq get_cpu(0x18), %rdi
    movq %rsp, %rax
    addq $(25 * 8), %rax
    movq %rax, 4(%rdi)
1:
    // Assume we're now on TASK2's stack
    // EOI
    irq_eoi_lapic 0

    irq_pop_ctx

    swapgs_if_needed
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
