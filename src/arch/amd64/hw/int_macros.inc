// vi:syntax=asm

.macro __int_push_ctx
    // pushal for amd64 world
    pushq %rax
    pushq %rcx
    pushq %rdx
    pushq %rbx
    pushq %rsp
    pushq %rbp
    pushq %rsi
    pushq %rdi

    // r8-r15
    pushq %r8
    pushq %r9
    pushq %r10
    pushq %r11
    pushq %r12
    pushq %r13
    pushq %r14
    pushq %r15

    // cr3
    mov %cr3, %rax
    pushq %rax

    // segments
    mov %gs, %rax
    pushq %rax
    mov %fs, %rax
    pushq %rax
    mov %es, %rax
    pushq %rax
    mov %ds, %rax
    pushq %rax
.endm
