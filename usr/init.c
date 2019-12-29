#include <sys/fcntl.h>
#include <signal.h>
#include <dirent.h>
#include <unistd.h>
#include <stdint.h>
#include <string.h>
#include <errno.h>
#include <stdio.h>

static void dummy_handler(int signum) {
    printf("Yay, no crash for this one\n");
}

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

    if (!strcmp(cmd, "pid")) {
        printf("%d\n", getpid());
        return 0;
    }

    if (!strcmp(cmd, "raise")) {
        if (raise(SIGABRT) != 0) {
            return -1;
        }
        return 0;
    }

    if (!strcmp(cmd, "crash")) {
        uint32_t *value_ptr = (uint32_t *) 0;
        *value_ptr = 123;
        return 0;
    }

    if (!strcmp(cmd, "fork-crash")) {
        int pid = fork();

        if (pid == 0) {
            printf("Crashing child\n");
            uint32_t *value_ptr = (uint32_t *) 0;
            *value_ptr = 123;
            printf("Shouldn't run\n");
            exit(-1);
        }
        return 0;
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

    if (!strcmp(cmd, "ls") || !strncmp(cmd, "ls ", 3)) {
        const char *path = cmd[2] ? cmd + 3 : "/";
        DIR *dir;
        struct dirent *ent;

        if (!(dir = opendir(path))) {
            perror(path);
            return -1;
        }

        while ((ent = readdir(dir))) {
            printf("%c %s\n", ent->d_type == DT_DIR ? 'D' : '-', ent->d_name);
        }

        closedir(dir);

        return 0;
    }

    printf("Unknown command: \"%s\"\n", cmd);

    return 0;
}

int main(int argc, char **argv) {
    char linebuf[512];
    char c;
    size_t l = 0;

    // Install dummy handler for SIGABRT
    signal(SIGABRT, dummy_handler);

    printf("> ");
    while (1) {
        if (read(STDIN_FILENO, &c, 1) < 0) {
            break;
        }

        if (c == '\n') {
            write(STDOUT_FILENO, &c, 1);
            linebuf[l] = 0;

            if (!strcmp(linebuf, "exit")) {
                break;
            }

            l = 0;
            cmd_exec(linebuf);
            printf("> ");
            continue;
        }

        linebuf[l++] = c;
        write(STDOUT_FILENO, &c, 1);
    }

    return -1;
}
