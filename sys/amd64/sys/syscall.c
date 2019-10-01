#include "sys/thread.h"
#include "sys/debug.h"

void amd64_syscall(uint64_t a0, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5) {
    switch (a0) {
    default:
        return;
    }
}
