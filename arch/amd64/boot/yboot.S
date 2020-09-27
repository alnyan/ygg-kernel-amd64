#include <config.h>
// For loader magic
#include <yboot2-proto/include/protocol.h>

.section .text
.global _entry_yboot
.type _entry_yboot, %function
_entry_yboot:
    cli

    leaq yboot_data(%rip), %rdi

    // Validate loader signature
    movq 8(%rdi), %rax
    movabsq $YB_LOADER_MAGIC_V1, %rsi
    cmp %rax, %rsi
    jne _bad_loader_magic

    // Setup upper paging
    leaq 1f(%rip), %rbp
    leaq boot_prepare_paging_64(%rip), %rdi
    jmp *%rdi
1:
    movq %rax, %cr3

    // Boot method 1
    mov $1, %rdi
    jmp kernel_pre_entry

_bad_loader_magic:
    cli
    hlt
    jmp _bad_loader_magic
.size _entry_yboot, . - _entry_yboot
