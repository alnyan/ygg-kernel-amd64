#include <sys/fcntl.h>
#include <unistd.h>
#include <stdint.h>
#include <errno.h>
#include <stdio.h>

static int cmd_exec(const char *cmd) {
    if (!strcmp(cmd, "fork")) {
        int pid = fork();

        if (pid == 0) {
            printf("Hello from child!\n");

            exit(1);
        } else {
            printf("Child's PID is %d\n", pid);

            return 0;
        }
    }

    if (!strcmp(cmd, "hello")) {
        int res;

        if ((res = fork()) == 0) {
            printf("In child\n");

            res = execve("/hello", NULL, NULL);

            if (res < 0) {
                perror("execve()");
            }

            exit(-1);
        }

        return 0;
    }

    printf("Unknown command: \"%s\"\n", cmd);

    return 0;
}

int main(int argc, char **argv) {
    char linebuf[512];
    char c;
    size_t l = 0;

    printf("> ");
    while (1) {
        if (read(STDIN_FILENO, &c, 1) < 0) {
            break;
        }

        if (c == '\n') {
            write(STDOUT_FILENO, &c, 1);
            linebuf[l] = 0;
            l = 0;
            cmd_exec(linebuf);
            printf("> ");
            continue;
        }

        linebuf[l++] = c;
        write(STDOUT_FILENO, &c, 1);
    }

    while (1);
}
