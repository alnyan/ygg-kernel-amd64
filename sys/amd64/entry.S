.section .text
// Args:
// %rdi - Address of loader's information struct
.global _start
_start:
    cli
    movabsq $kernel_stack_top, %rsp

    leaq _msg_entry(%rip), %rsi
    xorq %rdi, %rdi
    call debugf

1:
    cli
    hlt
    jmp 1b

.section .rodata
_msg_entry:
    .string "Entered kernel\n"

.section .bss
kernel_stack_bottom:
    .skip 65536
kernel_stack_top:
