.section .text
.global amd64_irq0
amd64_irq0:
    pushq %rax
    pushq %rcx
    pushq %rdx
    pushq %rbx

    pushq %rdi
    pushq %rsi

    // EOI
    movq $0xFFFFFF00FEE000B0, %rax
    movl $0, (%rax)

    popq %rsi
    popq %rdi

    popq %rbx
    popq %rdx
    popq %rcx
    popq %rax

    iretq
