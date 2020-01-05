#include <sys/fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <stdio.h>

#define LINE_LENGTH         16

static void line_print(size_t off, const char *line, size_t len) {
    printf("%08x: ", off);
    for (size_t i = 0; i < LINE_LENGTH; ++i) {
        // XXX: This is needed because I didn't implement h/hh modifiers in printf
        uint64_t byte = (uint64_t) line[i] & 0xFF;
        if (i < len) {
            printf("%02x", byte);
        } else {
            printf("  ");
        }
        if (i % 2) {
            printf(" ");
        }
    }
    printf("| ");
    for (size_t i = 0; i < len; ++i) {
        // TODO: isprint?
        if (line[i] >= ' ') {
            printf("%c", line[i]);
        } else {
            printf(".");
        }
    }
    printf("\n");
}

int main(int argc, char **argv) {
    int fd;
    const char *path;
    char buf[512];
    char line[LINE_LENGTH];
    ssize_t bread;
    size_t offset;
    size_t linel;
    size_t n_full_zero;

    if (argc != 2) {
        printf("wrong\n");
        return -1;
    }

    path = argv[1];
    printf("open file %s\n", path);

    if ((fd = open(path, O_RDONLY, 0)) < 0) {
        perror(path);
        return -1;
    }

    offset = 0;
    linel = 0;
    n_full_zero = 0;
    while ((bread = read(fd, buf, sizeof(buf))) > 0) {
        for (size_t i = 0; i < bread; ++i) {
            line[linel++] = buf[i];
            if (linel == LINE_LENGTH) {
                // Check if the line is all zeros
                int all_zeros = 1;
                for (size_t j = 0; j < LINE_LENGTH; ++j) {
                    if (line[j]) {
                        all_zeros = 0;
                        break;
                    }
                }
                if (all_zeros) {
                    ++n_full_zero;
                } else {
                    n_full_zero = 0;
                }

                if (n_full_zero < 3) {
                    line_print(offset, line, linel);
                } else if (n_full_zero == 3) {
                    printf(" ... \n");
                }
                offset += LINE_LENGTH;
                linel = 0;
            }
        }
    }
    if (linel) {
        line_print(offset, line, linel);
    }

    close(fd);

    return 0;
}
