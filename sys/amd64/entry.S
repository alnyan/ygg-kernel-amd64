.section .text
// Args:
// %rdi - Address of loader's information struct
.global _start
_start:
    cli
    leaq kernel_stack_top(%rip), %rsp

    call kernel_main

1:
    cli
    hlt
    jmp 1b

// Entrypoint for AP bootstrap code
.global ap_kernel_stacks_top

.section .bss
kernel_stack_bottom:
    .skip 65536
kernel_stack_top:

.set MAX_CPUS, 4
ap_kernel_stacks_bottom:
    .skip 65536 * (MAX_CPUS - 1)
ap_kernel_stacks_top:
