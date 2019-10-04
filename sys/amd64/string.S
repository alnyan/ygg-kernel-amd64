.section .text
.global strlen
strlen:
    // %rdi - str
    xorq %rax, %rax
1:
    cmpq $0, (%rdi)
    je 1f
    incq %rax
    incq %rdi
    jmp 1b
1:

    ret
