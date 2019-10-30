#include <sys/fcntl.h>
#include <unistd.h>
#include <stdint.h>
#include <errno.h>
#include <stdio.h>

#define BUF_SIZE        1024

int main(int argc, char **argv) {
    char c;
    char *buf;

    puts("Hello, World!");

    if (!(buf = malloc(BUF_SIZE))) {
        printf("Failed to alloc!\n");
        return -1;
    }

    struct stat test_stat;
    if (stat("/init", &test_stat) == 0) {
        printf("stat(\"/init\"):\n");
        printf(" st_size = %u\n", test_stat.st_size);
        printf(" st_mode = %d%d%d\n", (test_stat.st_mode >> 6) & 0x7,
                                      (test_stat.st_mode >> 3) & 0x7,
                                      test_stat.st_mode & 0x7);
    }

    if (stat("/1etc/file.txt", &test_stat) == 0) {
        printf("stat(\"/etc/file.txt\"):\n");
        printf(" st_size = %u\n", test_stat.st_size);
        printf(" st_mode = %d%d%d\n", (test_stat.st_mode >> 6) & 0x7,
                                      (test_stat.st_mode >> 3) & 0x7,
                                      test_stat.st_mode & 0x7);
    } else {
        perror("stat()");
    }

    int fd = open("/etc/file.txt", O_RDONLY, 0);

    if (fd >= 0) {
        printf("Opened /etc/file.txt\n");
        ssize_t count;

        if ((count = read(fd, buf, BUF_SIZE)) > 0) {
            printf("Read data from file:\n");
            write(STDOUT_FILENO, buf, count);
            printf("\n");
        }

        close(fd);
    }

    while (1) {
        if (read(STDIN_FILENO, &c, 1) < 0) {
            break;
        }

        if (c == '\n') {
            break;
        }

        printf("Typed: %c\n", c);
    }

    printf("Exited\n");

    return 0;
}
