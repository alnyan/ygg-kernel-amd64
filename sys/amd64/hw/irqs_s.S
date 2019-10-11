// Temporary file where I play with
// Generic IRQ handlers
#include "sys/amd64/asm/asm_irq.h"

.section .text

// This is keyboard
// Can be either entered from a legacy PIT IRQ
// Or is mapped in I/O APIC
.global amd64_irq1
amd64_irq1:
    cli

    irq_push_ctx
    movq %rsp, %rbp

    irq_trace 1

    // Read from keyboard buffer
    inb $0x60, %al

    irq_eoi_lapic 1

    irq_pop_ctx

    iretq

.global amd64_kstack_canary_invalid
#if defined(AMD64_STACK_CTX_CANARY)
amd64_kstack_canary_invalid:
    // This means we somehow managed to fuck up
    // Context's kernel stack and were going to
    // pop nonsense and iret would be fatal.

    // TODO: guess it would be just better to panic
    //       than to halt one CPU
    xorq %rdi, %rdi
    leaq _msg0(%rip), %rsi
    call debugs
1:
    cli
    hlt
    jmp 1b

_msg0:
    .string "Stack fuckup detected: halting\n"
#endif
