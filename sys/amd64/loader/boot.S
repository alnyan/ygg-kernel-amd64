.set ALIGN,     1 << 0
.set MEMINFO,   1 << 1
.set VIDEO,     1 << 2
.set FLAGS,     ALIGN | VIDEO | MEMINFO
.set MAGIC,     0x1BADB002
.set CHECKSUM,  -(MAGIC + FLAGS)

.set VIDEO_LFB,     0
.set VIDEO_EGA,     1
.set VIDEO_MODE,    VIDEO_LFB
.set VIDEO_WIDTH,   1920
.set VIDEO_HEIGHT,  1080
.set VIDEO_DEPTH,   32

.section .multiboot
.align 4
.long MAGIC
.long FLAGS
.long CHECKSUM
.long 0             // header_addr
.long 0             // load_addr
.long 0             // load_end_addr
.long 0             // bss_end_addr
.long 0             // entry_addr
.long VIDEO_MODE
.long VIDEO_WIDTH
.long VIDEO_HEIGHT
.long VIDEO_DEPTH

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

.global long_entry
long_entry:
    cli

    // Enable PSE and PAE
    mov %cr4, %eax
    or $((1 << 5) | (1 << 4)), %eax
    mov %eax, %cr4

    // Load PML4 address into cr3
    mov $pml4, %eax
    mov %eax, %cr3

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
    lgdt (gdtr64)
    ljmp $0x08, $.long_reload

.long_reload:
    mov $0x10, %ax
    mov %ax, %ds
    mov %ax, %es
    mov %ax, %fs
    mov %ax, %gs
    mov %ax, %ss

.code64
    leaq loader_data(%rip), %rdi
    movq 4(%rsp), %rax
    jmp *%rax
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
    .long gdt64
