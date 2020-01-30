#pragma once
#include "sys/amd64/asm/asm_apic.h"

#define AMD64_STACK_CTX_CANARY      0xBAD57AC6BAD57AC6
// TODO: use base + off instead of an absolute address

#if defined(__ASM__)
.extern local_apic
.extern irq_handle

.macro irq_entry, n
.global amd64_irq\n
amd64_irq\n:
    cli

    pushq %r11
    pushq %r10
    pushq %r9
    pushq %r8
    pushq %rcx
    pushq %rdx
    pushq %rsi
    pushq %rdi
    pushq %rax
    irq_eoi_lapic 0

    movq $\n, %rdi
    call irq_handle

    popq %rax
    popq %rdi
    popq %rsi
    popq %rdx
    popq %rcx
    popq %r8
    popq %r9
    popq %r10
    popq %r11
    iretq
.endm

.macro irq_eoi_lapic, n
    // n is actually ignored for APIC
    movq local_apic(%rip), %rax
    addq $LAPIC_REG_EOI, %rax
    movl $0, (%rax)
.endm
#else
// Externs for C code
extern void amd64_irq0_early();
extern void amd64_irq0();
extern void amd64_irq1();
extern void amd64_irq2();
extern void amd64_irq3();
extern void amd64_irq4();
extern void amd64_irq5();
extern void amd64_irq6();
extern void amd64_irq7();
extern void amd64_irq8();
extern void amd64_irq9();
extern void amd64_irq10();
extern void amd64_irq11();
extern void amd64_irq12();
extern void amd64_irq13();
extern void amd64_irq14();
extern void amd64_irq15();

#if defined(AMD64_MAX_SMP)
extern void amd64_irq_ipi();
extern void amd64_irq_ipi_panic();
#endif

extern void amd64_irq_msi0();
#endif
