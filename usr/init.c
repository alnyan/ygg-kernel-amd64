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
            char type;
            switch (ent->d_type) {
            case DT_REG:
                type = '-';
                break;
            case DT_DIR:
                type = 'D';
                break;
            case DT_BLK:
                type = 'b';
                break;
            case DT_CHR:
                type = 'c';
                break;
            default:
                type = '?';
                break;
            }
            printf("%c %s\n", type, ent->d_name);
        }

        closedir(dir);

        return 0;
    }

    if (!strncmp(cmd, "cat ", 4)) {
        const char *path = cmd + 4;
        char buf[512];
        int fd;
        ssize_t bread;

        if ((fd = open(path, O_RDONLY, 0)) < 0) {
            perror(path);
            return -1;
        }

        printf("--- Begin ---\n");
        while ((bread = read(fd, buf, sizeof(buf))) > 0) {
            write(STDOUT_FILENO, buf, bread);
        }
        printf("\n--- End ---\n");

        close(fd);

        return 0;
    }

    if (!strcmp(cmd, "walk")) {
        char c;
        printf("\033[s");

        while (read(STDIN_FILENO, &c, 1) >= 0) {
            if (c == 'q') {
                break;
            }

            switch (c) {
            case 'j':
                printf("\033[B");
                break;
            case 'k':
                printf("\033[A");
                break;
            case 'h':
                printf("\033[D");
                break;
            case 'l':
                printf("\033[C");
                break;
            case 'p':
                printf("\033[43;33m \033[0m\033[D");
                break;
            }
        }

        printf("\033[u");
        return 0;
    }

    printf("Unknown command: \"%s\"\n", cmd);

    return 0;
}

static void prompt(void) {
    printf("\033[36mygg\033[0m > ");
}

int main(int argc, char **argv) {
    char linebuf[512];
    char c;
    size_t l = 0;

    // Install dummy handler for SIGABRT
    signal(SIGABRT, dummy_handler);

    prompt();
    while (1) {
        if (read(STDIN_FILENO, &c, 1) < 0) {
            break;
        }

        if (c == '\b') {
            if (l) {
                linebuf[--l] = 0;
                printf("\033[D \033[D");
            }
            continue;
        }
        if (c == '\n') {
            write(STDOUT_FILENO, &c, 1);
            linebuf[l] = 0;

            if (!strcmp(linebuf, "exit")) {
                break;
            }

            l = 0;
            cmd_exec(linebuf);
            prompt();
            continue;
        }

        linebuf[l++] = c;
        write(STDOUT_FILENO, &c, 1);
    }

    return -1;
}
