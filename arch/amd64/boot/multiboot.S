#include <config.h>

// TODO: optional multiboot2 support
.section .multiboot
.set MAGIC,     0xE85250D6
.set ARCH,      0x0
.set HDRLEN,    16
.set CHKSUM,    -(MAGIC + ARCH + HDRLEN)

.long MAGIC
.long ARCH
.long HDRLEN
.long (CHKSUM & 0xFFFFFFFF)

#if defined(VESA_ENABLE)
// Framebuffer tag
.short 5
.short 0
.long 20
.long VESA_WIDTH
.long VESA_HEIGHT
.long VESA_DEPTH
#endif

// End tag
.short 0
.long 8

// Args:
// %rdi - Address of loader's information struct
.section .text
.global _entry_multiboot
.type _entry_multiboot, %function
_entry_multiboot:
.code32
    cli
    // Store multiboot info before doing anything
    leal (multiboot_registers - 0xFFFFFF0000000000), %edi
    movl %eax, 0(%edi)
    movl %ebx, 4(%edi)

    // Setup long mode
    // Enable PSE and PAE
    mov %cr4, %eax
    or $((1 << 5) | (1 << 4)), %eax
    mov %eax, %cr4

    // Prepare paging structures
    leal 1f - 0xFFFFFF0000000000, %ebp
    leal boot_prepare_paging_32 - 0xFFFFFF0000000000, %eax
    jmp *%eax
1:
    // Load PML4 into CR3
    movl %edi, %cr3

    // Enable EFER.LME
    mov $0xC0000080, %ecx
    rdmsr
    or $(1 << 8), %eax
    wrmsr

    // Enable paging
    mov %cr0, %eax
    or $(1 << 31), %eax
    mov %eax, %cr0

    // Load 64-bit GDT
    // Have to fix up the addresses because not in 64-bit mode yet
    lgdt (gdtr64 - 0xFFFFFF0000000000)
    ljmp $0x08, $(.long_reload - 0xFFFFFF0000000000)
.long_reload:
    mov $0x10, %ax
    mov %ax, %ds
    mov %ax, %es
    mov %ax, %fs
    mov %ax, %gs
    mov %ax, %ss

.code64
    // Boot method 0
    mov $0, %rdi
    jmp kernel_pre_entry
.size _entry_multiboot, . - _entry_multiboot

.code32
.align 16
gdt64:
    .quad 0
    .quad 0x00209A0000000000
    .quad 0x0000920000000000
gdt_end64:
.align 16
gdtr64:
    .short gdt_end64 - gdt64 - 1
    .long gdt64 - 0xFFFFFF0000000000

.section .bss
.global multiboot_registers
multiboot_registers:
    // %eax - Magic
    // %ebx - Multiboot2 struct phys addr
    .skip 8
