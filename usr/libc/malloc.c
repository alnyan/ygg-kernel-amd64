#include <unistd.h>
#include "bits/global.h"

// XXX: I'll implement a proper heap in the following commits
void *malloc(size_t count) {
    void *p = __cur_brk;
    if (sbrk(count) == p) {
        return NULL;
    }
    return p;
}

void free(void *ptr) {

}
