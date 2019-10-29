#include <unistd.h>
#include <stdint.h>

int main(int argc, char **argv) {
    char c;

    while (1) {
        ssize_t sz = read(STDIN_FILENO, &c, 1);

        if (sz >= 0) {
            if (c == '\n') {
                break;
            }

            write(STDOUT_FILENO, "Char: ", 6);
            write(STDOUT_FILENO, &c, 1);
            c = '\n';
            write(STDOUT_FILENO, &c, 1);
        }
    }

    write(STDOUT_FILENO, "Enter\n", 6);
    return 0;
}
