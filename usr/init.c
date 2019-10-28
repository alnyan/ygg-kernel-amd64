#include <unistd.h>
#include <stdint.h>

typedef uint64_t size_t;
typedef int64_t ssize_t;

void _start(void) {
    char c;

    while (1) {
        ssize_t sz = read(STDIN_FILENO, &c, 1);

        if (sz >= 0) {
            write(STDOUT_FILENO, "Char: ", 6);
            write(STDOUT_FILENO, &c, 1);
            c = '\n';
            write(STDOUT_FILENO, &c, 1);
        }
    }
}
