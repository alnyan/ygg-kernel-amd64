#include <unistd.h>
#include <string.h>
#include <stdio.h>

int puts(const char *s) {
    size_t l = strlen(s);
    int w = write(STDOUT_FILENO, s, l);
    char c = '\n';

    if (w == l) {
        write(STDOUT_FILENO, &c, 1);
    }
    return w;
}

int putchar(int c) {
    int res;
    if ((res = write(STDOUT_FILENO, &c, 1)) < 0) {
        return res;
    }
    return c;
}
