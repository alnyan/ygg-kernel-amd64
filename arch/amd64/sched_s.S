#include "arch/amd64/asm/asm_cpu.h"
#include "arch/amd64/asm/asm_thread.h"

.section .text
.global context_enter
context_enter:
    popq %rax
    popq %rdi
    popq %rsi
    popq %rdx
    popq %rcx
    popq %r8
    popq %r9
    popq %r10
    popq %r11

    swapgs_if_needed
    iretq

.global context_exec_enter
context_exec_enter:
    // %rdi - arg
    // %rsi - thread
    // %rdx - stack
    // %rcx - entry
    movq THREAD_RSP0(%rsi), %rsp
    movq THREAD_CR3(%rsi), %rax
    movq %rax, %cr3

    pushq $0x1B
    pushq %rdx

    pushq $0x200

    pushq $0x23
    pushq %rcx

    // Zero registers before entry
    xorq %rax, %rax
    xorq %rcx, %rcx
    xorq %rdx, %rdx
    xorq %rbx, %rbx

    xorq %rsi, %rsi
    xorq %rbp, %rbp

    xorq %r8, %r8
    xorq %r9, %r9
    xorq %r10, %r10
    xorq %r11, %r11
    xorq %r12, %r12
    xorq %r13, %r13
    xorq %r14, %r14
    xorq %r15, %r15

    swapgs_if_needed
    iretq

.global context_switch_first
.global context_switch_to
.type context_switch_to, %function
context_switch_to:
    // %rdi - new thread
    // %rsi - from
    pushq %rdi
    pushq %rsi

    call context_save_fpu

    popq %rsi
    popq %rdi

    pushq %r15
    pushq %r14
    pushq %r13
    pushq %r12
    pushq %rbp
    pushq %rbx

    movq %rsp, (%rsi)
context_switch_first:
    // TODO: switch cr3 here

    // Load new %rsp
    movq THREAD_RSP0(%rdi), %rsp

    popq %rbx
    popq %rbp
    popq %r12
    popq %r13
    popq %r14
    popq %r15

    pushq %rdi
    pushq %rsi

    call context_restore_fpu

    popq %rsi
    popq %rdi

    // Load TSS.RSP0 for user -> kernel transition
    // %rax = top of task's stack
    movq THREAD_RSP0_TOP(%rdi), %rax
    // %rcx = &tss
    movq get_cpu(CPU_TSS), %rcx
    // &tss->rsp0 = %rax
    movq %rax, TSS_RSP0(%rcx)

    // Load new %cr3 if changed
    movq THREAD_CR3(%rdi), %rax
    movq %cr3, %rcx
    test %rcx, %rax
    jz 1f
    // Only load if cr3 != thr->cr3
    movq %rax, %cr3
1:

    ret
.size context_switch_to, . - context_switch_to

.global context_sigreturn
context_sigreturn:
    movq get_cpu(CPU_THREAD), %rsp
    movq THREAD_RSP0_SIGENTER(%rsp), %rsp

    popq %rbx
    popq %rbp
    popq %r12
    popq %r13
    popq %r14
    popq %r15

    ret

.global context_sigenter
context_sigenter:
    // %rdi - entry
    // %rsi - stack
    // %rdx - argument
    pushq %r15
    pushq %r14
    pushq %r13
    pushq %r12
    pushq %rbp
    pushq %rbx

    // Use alternate TSS.RSP0 so syscall frame does not get overwritten
    // Current stack pointer becomes a new "top" of the stack, essentially
    // making it impossible for those scenarios to happen:
    // 1. userspace -> kernel ISR transition via TSS starts rewriting syscall/old ISR context
    // 2. syscalls issued from signal context overwriting normal context
    movq get_cpu(CPU_THREAD), %rbx
    movq %rsp, THREAD_RSP0_SIGENTER(%rbx)
    movq %rsp, THREAD_RSP0(%rbx)
    movq %rsp, THREAD_RSP0_TOP(%rbx)
    movq get_cpu(CPU_TSS), %r12
    movq %rsp, TSS_RSP0(%r12)

    // iretq frame
    pushq $0x1B
    pushq %rsi

    pushq $0x200

    pushq $0x23
    pushq %rdi

    // Move argument
    movq %rdx, %rdi

    swapgs
    iretq
