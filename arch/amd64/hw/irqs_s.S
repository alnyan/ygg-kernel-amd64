// Temporary file where I play with
// Generic IRQ handlers
#include "arch/amd64/asm/asm_irq.h"

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

.global amd64_irq_msi0
amd64_irq_msi0:
    cli
    iret_swapgs_if_needed

    pushq %r11
    pushq %r10
    pushq %r9
    pushq %r8
    pushq %rcx
    pushq %rdx
    pushq %rsi
    pushq %rdi
    pushq %rax
    irq_eoi_lapic 0

    movq $0, %rdi
    call amd64_msi_handle

    irq_eoi_lapic 0

    popq %rax
    popq %rdi
    popq %rsi
    popq %rdx
    popq %rcx
    popq %r8
    popq %r9
    popq %r10
    popq %r11

    iret_swapgs_if_needed
    iretq
