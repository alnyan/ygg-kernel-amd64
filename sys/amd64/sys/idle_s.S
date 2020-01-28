#include "sys/amd64/asm/asm_cpu.h"
#include "sys/amd64/asm/asm_irq.h"
.section .text

.global sched_switch_context
sched_switch_context:
    // Save caller-saved registers
    pushq %rbx
    pushq %r12
    pushq %r13
    pushq %r14
    pushq %r15
    pushq %rbp

    // %r12 = from
    movq get_cpu(0x08), %r12
    movq %rsp, (%r12)

    // %r12 = to
    //movq %rdi, %r12
    movq %rdi, get_cpu(0x08)

    movq (%rdi), %rsp

    // Write TSS
    movq %rsp, %r12
    addq $48, %r12
    movq get_cpu(0x18), %rdi
    movq %r12, 4(%rdi)

    popq %rbp
    popq %r15
    popq %r14
    popq %r13
    popq %r12
    popq %rbx

    sti
    ret

.global sched_enter_internal
sched_enter_internal:
    // %rdi - thread pointer
    // %rsi - entry function

    // Set cpu->thread
    movq %rdi, get_cpu(0x08)

    // %rsp = thread->kstack
    movq (%rdi), %rcx
    movq %rcx, %rsp

    //movq %rsp, %rcx
    //pushq $0x10
    //pushq %rcx
    //pushfq
    //pushq $0x08
    //leaq sched_idle_func(%rip), %rcx
    //pushq %rcx

    // Write TSS entry
    // movq get_cpu(0x18), %rdi
    // movq %rsp, %rax
    // addq $48, %rax
    // movq %rax, 4(%rdi)

    popq %rbp
    popq %r15
    popq %r14
    popq %r13
    popq %r12
    popq %rbx

    sti
    ret

    //iretq

//.section .bss
//some_stack:
//    .skip 4096
//.global sched_yield
//sched_yield:
//    // PUSH THREAD CONTEXT SOMEHOW?
//    movq (%rsp), %rax
//    movq %rsp, %rcx
//
//    pushq $0x10
//    pushq %rcx
//    pushq $0x248
//    pushq $0x08
//    pushq %rax
//
//    irq_push_ctx
//
//    call sched
//
//    // Load a new context
//    // rsi = thread structure pointer
//    movq get_cpu(0x08), %rsi
//    movq 0(%rsi), %rsp
//
//    // Write TSS entry
//    movq get_cpu(0x18), %rdi
//    movq %rsp, %rax
//    addq $(25 * 8), %rax
//    movq %rax, 4(%rdi)
//1:
//    // Assume we're now on TASK2's stack
//    irq_pop_ctx
//
//    iretq

