.section .text
.global boot_prepare_paging_32
// Args: %ebp - return address
// Return: %eax - physical PML4 address
// Sets up paging directory
boot_prepare_paging_32:
.code32
    leal (kernel_pd_res - 0xFFFFFF0000000000), %edi
    movl %edi, %ebx
    movl $0x1800, %ecx
    xorl %eax, %eax
    cld
    rep stosl

    leal (boot_prepare_common - 0xFFFFFF0000000000), %edi
    jmp *%edi

.global boot_prepare_paging_64
boot_prepare_paging_64:
.code64
    leaq kernel_pd_res(%rip), %rdi
    movq %rdi, %rbx
    movl $0x1800, %ecx
    xorl %eax, %eax
    cld
    rep stosl

boot_prepare_common:
    // %ebx - kernel_pd_res
.code32
    // Setup PDs
    movl %ebx, %edi
    xorl %ecx, %ecx
1:
    // PD[i] = (i << 21) | write | present | huge
    movl %ecx, %edx
    shl $21, %edx
    orl $((1 << 7) | (1 << 0) | (1 << 1)), %edx
    movl %edx, (%edi, %ecx, 8)

    add $1, %ecx
    cmpl $(512 * 4), %ecx
    jne 1b

    // Setup PDPT
    addl $(4096 * 4), %edi
    xorl %ecx, %ecx
2:
    // edi[ecx++] <- (ebx += 0x1000) | (1 << 0) | (1 << 1)
    movl %ebx, %edx
    orl $((1 << 0) | (1 << 1)), %edx
    movl %edx, (%edi, %ecx, 8)
    addl $0x1000, %ebx

    add $1, %ecx
    cmpl $4, %ecx
    jne 2b

    // Setup PML4
    // %edi - PDPT
    movl %edi, %ebx
    addl $0x1000, %edi
    orl $((1 << 0) | (1 << 1)), %ebx
    movl %ebx, (%edi)
    movl %ebx, 4080(%edi)

    mov %edi, %eax
    jmp *%ebp
.code64
