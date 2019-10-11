#include "sys/amd64/asm/asm_irq.h"

.section .text
.global amd64_irq_trace
amd64_irq_trace:
    pushq %rbp
    movq %rsp, %rbp

    // Get LAPIC ID
    // TODO: support non-APIC configs
    movq local_apic(%rip), %rax
    addq $LAPIC_REG_ID, %rax
    movl (%rax), %edx
    shrl $24, %edx

    leaq _irq_msg_fmt(%rip), %rsi
    movq %rdi, %rcx
    xorq %rdi, %rdi
    call debugf

    leaveq
    retq

_irq_msg_fmt:
    .string "[IRQ @ %d] %d\n"
