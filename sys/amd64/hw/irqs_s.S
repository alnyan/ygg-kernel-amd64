// Temporary file where I play with
// Generic IRQ handlers
#include "sys/amd64/asm/asm_irq.h"

.section .text

// TODO: move all this to some kind of generic handler
// This is keyboard
// Can be either entered from a legacy PIC IRQ
// Or is mapped in I/O APIC
.global amd64_irq1
amd64_irq1:
    cli

    irq_push_ctx
    movq %rsp, %rbp

    // The ISR is shared between legacy IRQs 1 and 4, so
    // check if it's keyboard or serial input on COM1
    // Check PS/2 keyboard
    inb $0x64, %al
    test $(1 << 0), %al
    jz 1f

    // Read from keyboard buffer
    inb $0x60, %al
    movb %al, irq1_key(%rip)

    jmp 2f
1:

    movq $0x3F8, %rdi
    call rs232_irq
    test %rax, %rax
    jz 2f

    // Only trace IRQ if it's unhandled
    irq_trace 1
2:
    irq_eoi_lapic 1

    irq_pop_ctx

    iretq


_amd64_irq1_unhandled:
    .string "Something unknown triggered ISR 0x21\n"
_amd64_serial:
    .string "Serial status: %02x\n"


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
