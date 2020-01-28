#include "sys/amd64/asm/asm_cpu.h"
#include "sys/amd64/asm/asm_irq.h"

.section .text
.global amd64_syscall_init
.global amd64_syscall_yield_stopped
.global amd64_syscall_iretq

#define MSR_STAR        0xC0000081
#define MSR_LSTAR       0xC0000082
#define MSR_SFMASK      0xC0000084
#define MSR_FSBASE      0xC0000100
#define MSR_GSBASE      0xC0000101
#define MSR_EFER        0xC0000080

// This is called once per each CPU
amd64_syscall_init:
    // LSTAR = amd64_syscall_entry
    movl $MSR_LSTAR, %ecx
    leaq amd64_syscall_entry(%rip), %rax
    movq %rax, %rdx
    shrq $32, %rdx
    wrmsr

    // SFMASK set to mask IF on entry
    movl $MSR_SFMASK, %ecx
    movl $(1 << 9), %eax
    xorl %edx, %edx
    wrmsr

    // STAR = ((ss3 - 8) << 48) | (cs0 << 32)
    movl $MSR_STAR, %ecx
    movl $(((0x1B - 8) << 16) | 0x08), %edx
    xorl %eax, %eax
    wrmsr

    // Enable SYSCALL instruction
    movl $MSR_EFER, %ecx
    rdmsr
    // EFER.SCE (bit 0)
    orl $1, %eax
    wrmsr

    retq

// Entrypoint of SYSCALL instruction
amd64_syscall_entry:
    // R11 = rflags3
    // RSP = rsp3
    // RCX = rip3

    swapgs

    // Check if full context has to be pushed onto the stack (fork())
    cmpq $57, %rax
    je _amd64_syscall_fork

    // // 1. Switch to kernel context
    // // 1.1. Store rsp3 in CPU struct
    movq %rsp, get_cpu(0x20)
    // 1.2. Load rsp0 from CPU struct
    movq get_cpu(0x08), %rsp
    movq (%rsp), %rsp

    pushq %rcx
    movq get_cpu(0x20), %rcx
    pushq %rcx
    pushq %r11

    // Userspace:
    //  %rdi %rsi %rdx %rcx %8 %r9
    // Syscall:
    //  %rdi %rsi %rdx %r10 %r8 %r9
    //  %rax
    // Need to move %r10 -> %rcx
    movq %r10, %rcx
    incq syscall_count(%rip)
    leaq amd64_syscall_jmp_table(%rip), %r10
    movq (%r10, %rax, 8), %r10
    test %r10, %r10
    jz 1f
    sti
    call *%r10
    jmp 2f
1:
    movq %rax, %rdi
    call sys_unknown
2:

    // Disable interrupts
    cli

    popq %r11
    popq %rdi
    popq %rcx

    // We could've been interrupted by irq0, which could've
    // scheduled a new task, which could've rewritten the TSS/RSP0
    // with weird values, so just fix that up
    movq %rsp, %rdx
    movq get_cpu(0x08), %rsi
    movq %rdx, (%rsi)
    movq get_cpu(0x18), %rsi
    addq $(25 * 8), %rdx
    movq %rdx, 4(%rsi)

    movq %rdi, %rsp

    swapgs
    sysretq

_amd64_syscall_fork:
    jmp .
//    // R11 = rflags3
//    // RSP = rsp3
//    // RCX = rip3
//    // Still on user stack
//    movq %rsp, get_cpu(0x20)
//    movq get_cpu(0x08), %rsp
//    movq (%rsp), %rsp
//
//    // Now on kernel stack
//    pushq $0x1B     // User SS
//    movq get_cpu(0x20), %rsi
//    pushq %rsi      // User RSP
//    pushq %r11      // User FLAGS
//    pushq $0x23     // User CS
//    pushq %rcx      // User RIP
//    // Now our kstack looks like we've just entered via an interrupt
//
//    irq_push_ctx
//
//    movq %rsp, %rbp
//
//    // Store the context we've just pushed
//    movq get_cpu(0x08), %rdi
//    test %rdi, %rdi
//    jz 1f
//    movq %rsp, (%rdi)
//1:
//    call sys_fork
//
//    // Well, we won't actually need this
//    // TODO: add irq_drop_ctx
//    irq_pop_ctx
//
//    swapgs
//    iretq


// Schedule control to a new task instead of stopped one
amd64_syscall_yield_stopped:
    jmp .
//    cli
//    // No stack frame - the kernel stack is going to be discarded anyways
//    movq get_cpu(0x08), %rbx
//
//    call sched
//    test %rax, %rax
//    jz 1f
//
//    // sched() failed
//    leaq _yield_failed(%rip), %rdi
//    xorq %rax, %rax
//    call panicf
//
//1:
//    // Remove old thread from queues
//    movq %rbx, %rdi
//    call sched_remove
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

_yield_failed:
    .string "sched() returned -1 on amd64_syscall_yield\n"

// Return from execve
amd64_syscall_iretq:
    cli
    // %rdi - %rsp0
    movq %rdi, %rsp

    // Write TSS entry
    movq get_cpu(0x18), %rdi
    movq %rsp, %rax
    addq $(25 * 8), %rax
    movq %rax, 4(%rdi)

    irq_pop_ctx
    swapgs
    iretq
