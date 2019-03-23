.section .text
.global _start
_start:
    cli
    movabsq $kernel_stack_top, %rsp

    call kernel_main

1:
    cli
    hlt
    jmp 1b

.section .bss
kernel_stack_bottom:
    .skip 65536
kernel_stack_top:
