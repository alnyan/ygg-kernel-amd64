#include <errno.h>
#include "bits/global.h"

extern char __heap_start;

void __libc_init(void) {
    __cur_brk = &__heap_start;
    errno = 0;
}
