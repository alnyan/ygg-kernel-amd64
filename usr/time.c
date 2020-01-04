#include <stdio.h>
#include <sys/time.h>

int main() {
    struct timeval tv;
    while (1) {
        usleep(100000);
        gettimeofday(&tv, NULL);

        printf("\033[s\033[1;70f%u\033[u", tv.tv_sec);
    }
}
