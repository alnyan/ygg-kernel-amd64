// vi: ft=asm :
.macro amd64_isr_nerr, n
amd64_exc_isr_\n:
    cli
    pushq $0
    pushq $\n
    jmp amd64_exc_generic
.endm

.macro amd64_isr_yerr, n
amd64_exc_isr_\n:
    cli
    pushq $\n
    jmp amd64_exc_generic
.endm

.section .text
amd64_exc_generic:
    // TODO: exception handling
    leaq _amd64_exc_str(%rip), %rsi
    movq (%rsp), %rdx
    xorq %rdi, %rdi
    call debugf

    cmpq $14, (%rsp)
    jne 1f

    movq %cr2, %rdx
    leaq _amd64_exc_pf(%rip), %rsi
    xorq %rdi, %rdi
    call debugf

1:
    cli
    hlt
    jmp 1b
_amd64_exc_str:
    .string "Error: unhandled exception: %d\n"
_amd64_exc_pf:
    .string "Page fault %p\n"

amd64_isr_nerr 0
amd64_isr_nerr 1
amd64_isr_nerr 2
amd64_isr_nerr 3
amd64_isr_nerr 4
amd64_isr_nerr 5
amd64_isr_nerr 6
amd64_isr_nerr 7
amd64_isr_yerr 8
amd64_isr_nerr 9
amd64_isr_yerr 10
amd64_isr_yerr 11
amd64_isr_yerr 12
amd64_isr_yerr 13
amd64_isr_yerr 14
amd64_isr_nerr 15
amd64_isr_nerr 16
amd64_isr_yerr 17
amd64_isr_nerr 18
amd64_isr_nerr 19
amd64_isr_nerr 20
amd64_isr_nerr 21
amd64_isr_nerr 22
amd64_isr_nerr 23
amd64_isr_nerr 24
amd64_isr_nerr 25
amd64_isr_nerr 26
amd64_isr_nerr 27
amd64_isr_nerr 28
amd64_isr_nerr 29
amd64_isr_yerr 30
amd64_isr_nerr 31

.section .rodata
.global amd64_exception_vectors
amd64_exception_vectors:
    .quad amd64_exc_isr_0
    .quad amd64_exc_isr_1
    .quad amd64_exc_isr_2
    .quad amd64_exc_isr_3
    .quad amd64_exc_isr_4
    .quad amd64_exc_isr_5
    .quad amd64_exc_isr_6
    .quad amd64_exc_isr_7
    .quad amd64_exc_isr_8
    .quad amd64_exc_isr_9
    .quad amd64_exc_isr_10
    .quad amd64_exc_isr_11
    .quad amd64_exc_isr_12
    .quad amd64_exc_isr_13
    .quad amd64_exc_isr_14
    .quad amd64_exc_isr_15
    .quad amd64_exc_isr_16
    .quad amd64_exc_isr_17
    .quad amd64_exc_isr_18
    .quad amd64_exc_isr_19
    .quad amd64_exc_isr_20
    .quad amd64_exc_isr_21
    .quad amd64_exc_isr_22
    .quad amd64_exc_isr_23
    .quad amd64_exc_isr_24
    .quad amd64_exc_isr_25
    .quad amd64_exc_isr_26
    .quad amd64_exc_isr_27
    .quad amd64_exc_isr_28
    .quad amd64_exc_isr_29
    .quad amd64_exc_isr_30
    .quad amd64_exc_isr_31
