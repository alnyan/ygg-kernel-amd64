#include <config.h>

.section .data.boot, "aw", %progbits
#define BOOT_KERNEL_MAGIC           0xA197A9B007B007UL
#define BOOT_LOADER_MAGIC           0x700B700B9A791AUL

.global yboot_data
.type yboot_data, %object
yboot_data:
    .quad BOOT_KERNEL_MAGIC     // kernel_magic
    .quad 0                     // loader_magic

    .quad 0                     // memory_map_base
    .long 0                     // memory_map_size
    .long 0                     // memory_map_entsize

    .long 1600                  // video_width
    .long 900                   // video_height
    .long 1                     // video_format = BGR
    .long 0                     // --
    .quad 0                     // video_framebuffer
    .quad 0                     // video_pitch

    .quad 0                     // elf_symtab_hdr
    .quad 0                     // elf_symtab_data
    .quad 0                     // elf_strtab_hdr
    .quad 0                     // elf_strtab_data

    .quad 0                     // initrd_base
    .quad 0                     // initrd_size

    .quad 0                     // rsdp

    .skip 256                   // cmdline
.size yboot_data, . - yboot_data

.section .text
.global _entry_yboot
.type _entry_yboot, %function
_entry_yboot:
    cli

    leaq yboot_data(%rip), %rdi

    // Validate loader signature
    movq 8(%rdi), %rax
    movabsq $BOOT_LOADER_MAGIC, %rsi
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
