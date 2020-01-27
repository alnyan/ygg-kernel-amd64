// Temporary file where I play with
// Generic IRQ handlers
#include "sys/amd64/asm/asm_irq.h"

.section .text

// 33
irq_entry 1
irq_entry 2
irq_entry 3
irq_entry 4
irq_entry 5
irq_entry 6
irq_entry 7
irq_entry 8
irq_entry 9
irq_entry 10
irq_entry 11
irq_entry 12
irq_entry 13
irq_entry 14
irq_entry 15

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

.global amd64_idt_load
amd64_idt_load:
    lidt (%rdi)
    ret
