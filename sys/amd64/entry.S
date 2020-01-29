.section .text
// Args:
// %rdi - Address of loader's information struct
.type _start, %function
.global _start
_start:
    cli
    leaq kernel_stacks_bottom(%rip), %rsp
    addq $AMD64_KERNEL_STACK, %rsp

    movq %rdi, %rbx
    call _init
    movq %rbx, %rdi

    xorq %rbp, %rbp
    pushq %rbp
    call kernel_main

1:
    cli
    hlt
    jmp 1b
.size _start, . - _start

// Entrypoint for AP bootstrap code
.global kernel_stacks_top

.section .bss
kernel_stacks_bottom:
#if defined(AMD64_SMP)
    .skip AMD64_KERNEL_STACK * AMD64_MAX_SMP
#else
    .skip AMD64_KERNEL_STACK
#endif
kernel_stacks_top:
