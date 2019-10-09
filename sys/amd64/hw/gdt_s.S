.section .text
.global amd64_gdt_load
amd64_gdt_load:
    lgdt (%rdi)
    // Not sure if CS reload is needed
    retq
