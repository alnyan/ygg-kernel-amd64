#include <config.h>

.set MULTIBOOT_LOADER_MAGIC, 0x36d76289

.section .text
.global _entry_common
.type _entry_common, %function
_entry_common:
.code32
    // Guess boot method by checking if
    // %eax has loader magic value
    movl $0x36d76289, %edx
    cmp %eax, %edx
    je 1f
    jmp _entry_yboot
1:
    leal _entry_multiboot - 0xFFFFFF0000000000, %edx
    jmp *%edx
.code64
.size _entry_common, . - _entry_common

.global kernel_pre_entry
.type kernel_pre_entry, %function
kernel_pre_entry:
    // Full 64-bit now
    leaq kernel_stacks_bottom(%rip), %rsp
    addq $8192, %rsp

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
.size kernel_pre_entry, . - kernel_pre_entry

.global kernel_stacks_top
.global kernel_stacks_bottom
kernel_stacks_bottom:
#if defined(AMD64_SMP)
    .skip AMD64_KERNEL_STACK * AMD64_MAX_SMP
#else
    .skip AMD64_KERNEL_STACK
#endif
kernel_stacks_top:
