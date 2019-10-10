#include "asm/asm_irq.h"
.section .text

.global amd64_irq0
amd64_irq0:
    // Interrupt entry:
    // rsp should be one of the following:
    //  The one specified in the TSS.RSP0
    //  When interrupting kernel code - Task's kernel RSP
    irq_push_ctx

    // Let's adhere to the amd64 ABI
    movq %rsp, %rbp

    // TODO: call sched here, do something

    // EOI
    irq_eoi_lapic 0

    irq_pop_ctx
    iretq

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
