.section .text
.global _start
_start:
0:
    movq $1, %rdi
    movq $2, %rsi
    movq $3, %rdx
    movq $4, %r10
    movq $5, %r8
    movq $6, %r9
    syscall

    movq $10000000, some_symbol(%rip)
1:
    decq some_symbol(%rip)
    jnz 1b
    jmp 0b

.section .data
.global some_symbol
some_symbol:
    .quad 0
