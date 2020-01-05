#include <errno.h>
#include "bits/global.h"

extern char __heap_start;
extern void __libc_signal_init(void);
static const char *argp_dummy[] = { 0 };

void __libc_init(char **argp) {
    __libc_signal_init();
    __libc_argc = 0;
    if (argp) {
        while (argp[__libc_argc]) {
            ++__libc_argc;
        }
    }
    __cur_brk = &__heap_start;
    errno = 0;
}
