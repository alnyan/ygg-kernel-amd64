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

    //push %ebx
    //push %eax

    // Check if the CPU supports long mode
    mov $0x80000001, %eax
    cpuid
    test $(1 << 29), %edx
    jz .not_supported

    // call loader_entry
    // add $8, %esp
1:
    cli
    hlt
    jmp 1b
.not_supported:
    jmp 1b
