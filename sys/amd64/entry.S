.section .text
// Args:
// %rdi - Address of loader's information struct
.global _start
_start:
    cli
    leaq kernel_stack_top(%rip), %rsp

    call kernel_main

1:
    cli
    hlt
    jmp 1b

.section .bss
kernel_stack_bottom:
    .skip 65536
kernel_stack_top:
