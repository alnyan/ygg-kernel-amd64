#include "sys/amd64/asm/asm_cpu.h"

.section .text
.global amd64_syscall_init

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

    // // 1. Switch to kernel context
    // // 1.1. Store rsp3 in CPU struct
    movq %rsp, get_cpu(0x20)
    // 1.2. Load rsp0 from CPU struct
    movq get_cpu(0x08), %rsp
    movq (%rsp), %rsp

    pushq %rax
    movq get_cpu(0x20), %rax
    pushq %rax

    movq $100000000, %rax
    sti

1:
    decq %rax
    jnz 1b
    cli

    popq %rdi
    popq %rax
    movq %rdi, %rsp

    swapgs
    sysretq
