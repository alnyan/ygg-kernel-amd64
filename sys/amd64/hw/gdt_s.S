.section .text
.global amd64_gdt_load
amd64_gdt_load:
    lgdt (%rdi)
    // Not sure if CS reload is needed
    movw $0x28, %ax
    ltr %ax
    retq
