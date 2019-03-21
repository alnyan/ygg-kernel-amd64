.set ALIGN,     1 << 0
.set MEMINFO,   1 << 1
.set FLAGS,     ALIGN | MEMINFO
.set MAGIC,     0x1BADB002
.set CHECKSUM,  -(MAGIC + FLAGS)

.section .multiboot
.align 4
.long MAGIC
.long FLAGS
.long CHECKSUM

.section .bss
stack_bottom:
    .skip 65536
stack_top:

.section .text
.global _start
_start:
    // Setup stack
    mov $stack_top, %esp

    push %ebx
    push %eax

    // Check if the CPU supports long mode
    mov $0x80000001, %eax
    cpuid
    test $(1 << 29), %edx
    jz .not_supported

    call loader_main
    add $8, %esp

    push $_msg_failure
    call panic
    add $4, %esp
1:
    cli
    hlt
    jmp 1b
.not_supported:
    push $_msg_not_supported
    call panic
    add $4, %esp
    jmp 1b

_msg_not_supported:
    .string "Long mode is not supported by the CPU"
_msg_failure:
    .string "Unknown error"
