.section .text
.global _start
.type _start, %function
// Arguments:
// %rdi - argp
_start:
    movq $0, %rbp
    // For callee linkage
    pushq %rbp
    pushq %rbp
    movq %rsp, %rbp

    push %rdi
    call __libc_init

    call _init

    mov __libc_argc, %rdi
    pop %rsi

    // exit(main(argc, argv))
    call main
    movq %rax, %rdi
    call exit
.size _start, . - _start
