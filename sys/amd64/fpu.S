.section .text
.global amd64_fpu_init
amd64_fpu_init:
    // Disable FPU software emulation bit
    // Set MP (monitor coprocessor) bit
    mov %cr0, %rax
    and $~(1 << 2), %rax
    or $(1 << 1), %rax
    mov %rax, %cr0

    // Set OSFXR (1 << 9, OS support for fxsave/fxrstor)
    // Set OSXMMEXCPT (1 << 10, OS support for unmasked simd float exceptions)
    mov %cr4, %rax
    or $((1 << 9) | (1 << 10)), %rax
    mov %rax, %cr4

    retq
