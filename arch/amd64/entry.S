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
    // Setup long mode
    // Enable PSE and PAE
    mov %cr4, %eax
    or $((1 << 5) | (1 << 4)), %eax
    mov %eax, %cr4

    cli
    hlt

.code64
    leaq kernel_stacks_bottom(%rip), %rsp
    addq $AMD64_KERNEL_STACK, %rsp

    cli
    hlt

    movq %rdi, %rbx
    call _init
    movq %rbx, %rdi

    cli
    hlt

    xorq %rbp, %rbp
    pushq %rbp
    call kernel_main

1:
    cli
    hlt
    jmp 1b
.size _entry64, . - _entry64

// Entrypoint for AP bootstrap code
.global kernel_stacks_top

.section .bss
kernel_stacks_bottom:
#if defined(AMD64_SMP)
    .skip AMD64_KERNEL_STACK * AMD64_MAX_SMP
#else
    .skip AMD64_KERNEL_STACK
#endif
kernel_stacks_top:
