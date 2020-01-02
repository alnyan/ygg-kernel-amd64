#include <bits/syscall.h>
#include <signal.h>
#include <stdio.h>

static sighandler_t signal_handlers[16] = {0};

void SIG_IGN(int signum) {
    printf("Ignored: %d\n", signum);

    while (1);
}

void SIG_DFL(int signum) {
    int pid = getpid();

    switch (signum) {
    case SIGKILL:
        printf("%d: killed\n", pid);
        break;
    case SIGABRT:
        printf("%d: aborted\n", pid);
        break;
    case SIGSEGV:
        printf("%d: segmentation fault\n", pid);
        break;
    default:
        printf("%d: signal %d\n", pid, signum);
        break;
    }

    exit(1);
}

static void __libc_signal_handle(int signum) {
    if (signum >= 16) {
        SIG_DFL(signum);
    } else {
        signal_handlers[signum](signum);
    }

    __kernel_sigret();
}

void __libc_signal_init(void) {
    for (size_t i = 0; i < 16; ++i) {
        signal_handlers[i] = SIG_DFL;
    }

    __kernel_signal((uintptr_t) __libc_signal_handle);
}

sighandler_t signal(int signum, sighandler_t new_handler) {
    sighandler_t old_handler;

    if (signum >= 16) {
        printf("Fuck\n");
        exit(1);
    }

    old_handler = signal_handlers[signum];
    signal_handlers[signum] = new_handler;
    return old_handler;
}

int raise(int signum) {
    return kill(getpid(), signum);
}

void abort(void) {
    kill(getpid(), SIGABRT);
}
