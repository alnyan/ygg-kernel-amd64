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

    iretq

.global context_switch_first
.global context_switch_to
context_switch_to:
    // %rdi - new thread
    // %rsi - from
    cli

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
    movq (%rdi), %rsp

    popq %rbx
    popq %rbp
    popq %r12
    popq %r13
    popq %r14
    popq %r15

    ret
