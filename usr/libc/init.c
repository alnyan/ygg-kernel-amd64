#include <errno.h>

void __libc_init(void) {
    errno = 0;
}
