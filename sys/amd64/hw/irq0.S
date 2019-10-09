.section .text
.global amd64_irq0
amd64_irq0:
    pushq %rax
    movq $0xFFFFFF00FEE000B0, %rax
    movl $0, (%rax)
    popq %rax

    pushq %rax
    pushq %rcx
    pushq %rdx
    pushq %rbx

    pushq %rdi
    pushq %rsi

    xorq %rdi, %rdi
    leaq _tmr_msg(%rip), %rsi
    call debugs

    popq %rsi
    popq %rdi

    popq %rbx
    popq %rdx
    popq %rcx
    popq %rax

    iretq

_tmr_msg:
    .string "T!\n"
