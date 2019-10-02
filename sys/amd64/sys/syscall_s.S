.set AMD64_MSR_EFER,  0xC0000080
.set AMD64_MSR_STAR,  0xC0000081
.set AMD64_MSR_LSTAR, 0xC0000082
.set AMD64_MSR_SFMASK, 0xC0000084

.extern amd64_syscall

.global amd64_setup_syscall
amd64_setup_syscall:
    // Load STAR = 0x08 << 32
    // Needed:
    // userCS = 0x23
    // userSS = 0x1B
    // SYSRET.CS = SYSRET.SS + 8 = X + 16 = 0x13 + 16
    // Needed:
    // kernCS = 0x08
    // kernSS = 0x10
    // SYSCALL.SS = SYSCALL.CS + 8 = 0x08 + 8
    movl $0x130008, %edx
    xorl %eax, %eax
    movl $AMD64_MSR_STAR, %ecx
    wrmsr

    // Load LSTAR = RIP of syscall entry
    leaq amd64_syscall_entry(%rip), %rdx
    movl %edx, %eax
    shrq $32, %rdx
    movl $AMD64_MSR_LSTAR, %ecx
    wrmsr

    movl $0x0200, %eax
    movl $0x0, %edx
    movl $AMD64_MSR_SFMASK, %ecx
    wrmsr

    // Set EFER.SCE = 1
    movl $AMD64_MSR_EFER, %ecx
    rdmsr
    or $1, %eax
    wrmsr

    retq

amd64_syscall_entry:
    // R11 = RFLAGS
    // RCX = userRIP
    // RSP = userRSP
    // Args:
    // %rdi, %rsi, %rdx, %r10, %r8, %r9
    // Preserved:
    // %r12-%r15, %rbx
    // Result:
    // %rax

    // 1. Load kernRSP from amd64_thread_current->kstack_ptr,
    //    userRSP is stored in %rax before being pushed
    movq %rsp, %r9
    movq sched_current(%rip), %rsp
    movq (%rsp), %rsp

    // syscall handlers may mess up this in SysV amd64 ABI, store
    // these on stack
    pushq %r9
    pushq %r11
    pushq %rcx

    // Move arguments
    movq %r10, %rcx
    movq %rax, %r9

    // Args:
    // %rdi %rsi, %rdx, %rcx
    call amd64_syscall

    popq %rcx
    popq %r11
    popq %rsp

    rex.w sysret
