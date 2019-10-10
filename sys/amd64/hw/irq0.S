.section .text
#define AMD64_STACK_CTX_CANARY 0xBAD57AC6BAD57AC6

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

    movq %ds, %rax
    pushq %rax
    movq %es, %rax
    pushq %rax
    movq %fs, %rax
    pushq %rax
    movq %gs, %rax
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
    movq %rax, %gs
    popq %rax
    movq %rax, %fs

    popq %rax
    movq %rax, %es
    popq %rax
    movq %rax, %ds

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

.global amd64_irq0
amd64_irq0:
    // Interrupt entry:
    // rsp should be one of the following:
    //  The one specified in the TSS.RSP0
    //  When interrupting kernel code - Task's kernel RSP
    irq_push_ctx

    // Let's adhere to the amd64 ABI
    movq %rsp, %rbp

    // TODO: call sched here, do something

    // EOI
    movq $0xFFFFFF00FEE000B0, %rax
    movl $0, (%rax)

    irq_pop_ctx
    iretq

#if defined(AMD64_STACK_CTX_CANARY)
amd64_kstack_canary_invalid:
    // This means we somehow managed to fuck up
    // Context's kernel stack and were going to
    // pop nonsense and iret would be fatal.

    // TODO: guess it would be just better to panic
    //       than to halt one CPU
    xorq %rdi, %rdi
    leaq _msg0(%rip), %rsi
    call debugs
1:
    cli
    hlt
    jmp 1b

_msg0:
    .string "Stack fuckup detected: halting\n"
#endif
