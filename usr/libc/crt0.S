.section .text
.global _start
.type _start, %function
// Arguments:
// %rdi - argc (well, not quite yet)
// %rsi - argv
_start:
    movq $0, %rbp
    // For callee linkage
    pushq %rbp
    pushq %rbp
    movq %rsp, %rbp

    call _init

    // Ignore %rdi/%rsi for now
    xorq %rdi, %rdi

    call main

    // I haven't implemented exit() yet
    xorq %rdi, %rdi
    jmp *%rdi
.size _start, . - _start
