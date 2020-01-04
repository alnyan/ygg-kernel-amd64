#include "time.h"
#include "bits/syscall.h"

int usleep(long us) {
    struct timespec tv = { us / 1000000, (us % 1000) * 1000 };
    return nanosleep(&tv, NULL);
}
