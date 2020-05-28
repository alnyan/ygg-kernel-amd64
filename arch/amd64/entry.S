.section .multiboot
.set MAGIC,     0xE85250D6
.set ARCH,      0x0
.set HDRLEN,    16
.set CHKSUM,    -(MAGIC + ARCH + HDRLEN)

.long MAGIC
.long ARCH
.long HDRLEN
.long CHKSUM
.long 0
.long 8

.section .text
// Args:
// %rdi - Address of loader's information struct
.type _entry64, %function
.global _entry64
_entry64:
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

    // 0x1FF000 - PML4
    // 0x1FE000 - PDPT
    movl $0x1FE000, %ebx
    // Zero out the tables
    movl %ebx, %edi
    movl $0x800, %ecx
    xorl %eax, %eax
    cld
    rep stosl

    // Setup PML4
    // PDPT | present | write
    orl $((1 << 1) | (1 << 0)), %ebx
    // PML4[0]   -> ...
    movl $0x1FF000, %edi
    movl %ebx, (%edi)
    // PML4[510] -> ...
    movl $0x1FFFF0, %edi
    movl %ebx, (%edi)

    // Setup PDPT
    movl $0x1FE000, %edi
    movl $0, %ecx
1:
    // PDPT[i] = (i << 30) | write | present | huge | global
    movl %ecx, %edx
    shl $30, %edx
    orl $((1 << 7) | (1 << 1) | (1 << 0)), %edx
    movl %edx, (%edi, %ecx, 8)

    incl %ecx
    cmpl $4, %ecx
    jne 1b

    // Load PML4 into CR3
    movl $0x1FF000, %eax
    movl %eax, %cr3

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
    // Full 64-bit now
    leaq kernel_stacks_bottom(%rip), %rsp
    addq $AMD64_KERNEL_STACK, %rsp

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
.size _entry64, . - _entry64

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

// Entrypoint for AP bootstrap code
.global kernel_stacks_top
.global multiboot_registers

.section .bss
multiboot_registers:
    // %eax - Magic
    // %ebx - Multiboot2 struct phys addr
    .skip 8
kernel_stacks_bottom:
#if defined(AMD64_SMP)
    .skip AMD64_KERNEL_STACK * AMD64_MAX_SMP
#else
    .skip AMD64_KERNEL_STACK
#endif
kernel_stacks_top:
