.section .text
.global spin_lock
.global spin_release

// TODO: maybe add some tracing to this so I know
//       the locks that consume too much CPU time
spin_lock:
    // %rdi - spinlock

    lock bts $1, (%rdi)
    jc 1f
    retq

    // Failed to obtain lock immediately, wait
1:
    pause
    testq $1, (%rdi)
    jnz 1b
    jmp spin_lock


spin_release:
    movq $0, (%rdi)
    retq
