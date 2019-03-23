.section .text
.global amd64_reload_segs
amd64_reload_segs:
    cli

    movabsq $.jmpbuf, %rax
    rex.w ljmp *(%rax)

.reload_data:
    mov $0x10, %ax
    mov %ax, %ds
    mov %ax, %es
    mov %ax, %fs
    mov %ax, %gs
    mov %ax, %ss

    ret

.jmpbuf:
    .quad .reload_data
    .word 0x08
