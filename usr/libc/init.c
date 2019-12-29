#include <errno.h>
#include "bits/global.h"

extern char __heap_start;
extern void __libc_signal_init(void);

void __libc_init(void) {
    __libc_signal_init();
    __cur_brk = &__heap_start;
    errno = 0;
}
