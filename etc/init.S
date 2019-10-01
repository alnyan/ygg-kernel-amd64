.section .text
.global _start
_start:
    movq $100, %rcx
1:
    dec %rcx
    jnz 1b
    // XXX: Syscall fucks up kstack?
    //syscall
    jmp _start
