#pragma once
#include "sys/amd64/asm/asm_apic.h"

#define AMD64_STACK_CTX_CANARY      0xBAD57AC6BAD57AC6
// TODO: use base + off instead of an absolute address

#if defined(__ASM__)
.extern local_apic
.extern irq_handle

// TODO: make this use less memory
.macro irq_entry, n
.global amd64_irq\n
amd64_irq\n:
    cli

    irq_push_ctx

    movq $\n, %rdi
    call irq_handle

    irq_eoi_lapic 0
    irq_pop_ctx

    iretq
.endm

.macro irq_trace, n
#if defined(AMD64_TRACE_IRQ)
    // Assuming we're entering from safe context
    movq $\n, %rdi
    call amd64_irq_trace
#else
#endif
.endm

.macro irq_push_ctx
    pushq %rax
    pushq %rcx
    pushq %rdx
    pushq %rbx

    // Don't push rsp - it's ignored
    pushq %rbp
    pushq %rsi
    pushq %rdi

    pushq %r8
    pushq %r9
    pushq %r10
    pushq %r11
    pushq %r12
    pushq %r13
    pushq %r14
    pushq %r15

    movq %cr3, %rax
    pushq %rax

    movq %ds, %rax
    pushq %rax
    movq %es, %rax
    pushq %rax
    movq %fs, %rax
    pushq %rax

#if defined(AMD64_STACK_CTX_CANARY)
    movq $AMD64_STACK_CTX_CANARY, %rax
    pushq %rax
#endif
.endm

.macro irq_pop_ctx
#if defined(AMD64_STACK_CTX_CANARY)
    movq $AMD64_STACK_CTX_CANARY, %rax
    cmpq (%rsp), %rax
    jne amd64_kstack_canary_invalid
    popq %rax
#endif

    // XXX: should they be popped this way?
    popq %rax
    //movq %rax, %fs

    popq %rax
    movq %rax, %es
    popq %rax
    movq %rax, %ds

    popq %rax
    movq %rax, %cr3

    popq %r15
    popq %r14
    popq %r13
    popq %r12
    popq %r11
    popq %r10
    popq %r9
    popq %r8

    popq %rdi
    popq %rsi
    popq %rbp

    popq %rbx
    popq %rdx
    popq %rcx
    popq %rax
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
