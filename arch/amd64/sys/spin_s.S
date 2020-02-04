.section .text
.global spin_lock
.global spin_release
.global spin_lock_irqsave
.global spin_release_irqrestore

// TODO: maybe add some tracing to this so I know
//       the locks that consume too much CPU time
spin_lock_irqsave:
    pushfq
    popq %rax
    movq %rax, (%rsi)
    cli

spin_lock:
    // %rdi - spinlock
    lock bts $0, (%rdi)
    jc 1f
    retq

    // Failed to obtain lock immediately, wait
1:
    pause
    testq $1, (%rdi)
    jnz 1b
    jmp spin_lock

spin_release_irqrestore:
    movq $0, (%rdi)
    testq $(1 << 9), (%rsi)
    jz 1f
    sti
1:
    retq

spin_release:
    movq $0, (%rdi)
    retq
