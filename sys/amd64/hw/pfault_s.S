#include "sys/amd64/asm/asm_irq.h"
#include "sys/amd64/asm/asm_cpu.h"

.section .text
.global amd64_exc_isr_14
amd64_exc_isr_14:
    cli
    // Stack:
    // error_code
    // ss
    // rsp
    // rflags
    // cs
    // rip
    swapgs_if_needed

    // Fuck error code
    addq $8, %rsp

    pushq %rax
    movq $(1 << 63), %rax
    testq %rax, 8(%rsp)
    jnz _pfault_kernel
    popq %rax

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

    movq %cr2, %rdi
    call amd64_pfault
    // Failed to handle the fault
1:
    cli
    hlt
    jmp 1b

_pfault_kernel:
    movq 16(%rsp), %rdi
    movq %cr2, %rsi
    call amd64_pfault_kernel
1:
    cli
    hlt
    jmp 1b
