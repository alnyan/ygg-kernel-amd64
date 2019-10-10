.section .text
// Args:
// %rdi - Address of loader's information struct
.global _start
_start:
    cli
    leaq kernel_stacks_bottom(%rip), %rsp
    addq $AMD64_KERNEL_STACK, %rsp

    call kernel_main

1:
    cli
    hlt
    jmp 1b

// Entrypoint for AP bootstrap code
.global kernel_stacks_top

.section .bss
kernel_stacks_bottom:
    .skip AMD64_KERNEL_STACK * AMD64_MAX_SMP
kernel_stacks_top:
