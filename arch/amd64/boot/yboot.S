#include <config.h>
#include <yboot/include/protocol.h>

.section .data.boot, "aw", %progbits

.global yboot_data
.type yboot_data, %object
yboot_data:
    .quad YB_KERNEL_MAGIC_V1                            // kernel_magic
    .quad 0                                             // loader_magic

    .quad yboot_memory_map_data - 0xFFFFFF0000000000    // memory_map_data
    .long 32768                                         // memory_map_size
    .long 0                                             // memory_map_entsize

    .long 640                                           // video_width
    .long 480                                           // video_height
    .long 1                                             // video_format = BGR
    .long 0                                             // --
    .quad 0                                             // video_framebuffer
    .quad 0                                             // video_pitch

    .quad 0                                             // elf_symtab_hdr
    .quad 0                                             // elf_symtab_data
    .quad 0                                             // elf_strtab_hdr
    .quad 0                                             // elf_strtab_data

    .quad 0                                             // initrd_base
    .quad 0                                             // initrd_size

    .quad 0                                             // rsdp

    .skip YB_CMDLINE_SIZE                               // cmdline
.size yboot_data, . - yboot_data

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
