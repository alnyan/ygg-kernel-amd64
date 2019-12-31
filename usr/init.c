#include <sys/fcntl.h>
#include <signal.h>
#include <dirent.h>
#include <unistd.h>
#include <stdint.h>
#include <string.h>
#include <errno.h>
#include <stdio.h>

// Cursor helpers

#define curs_save() \
    write(STDOUT_FILENO, "\033[s", 3)
#define curs_unsave() \
    write(STDOUT_FILENO, "\033[u", 3)
#define curs_set(row, col) \
    printf("\033[%d;%df", row, col)
#define clear() \
    write(STDOUT_FILENO, "\033[2J", 4)

//

extern __attribute__((noreturn)) void abort(void);

#define assert(x) \
    if (!(x)) abort()

struct builtin {
    const char *name;
    const char *desc;
    int (*run) (const char *arg);
};

static int b_ls(const char *path);
static int b_cd(const char *path);
static int b_pwd(const char *_);
static int b_cat(const char *path);
static int b_curs(const char *arg);
static int b_sleep(const char *arg);
static int b_help(const char *arg);

static struct builtin builtins[] = {
    {
        "ls",
        "List files in directory",
        b_ls,
    },
    {
        "cd",
        "Change working directory",
        b_cd,
    },
    {
        "pwd",
        "Print working directory",
        b_pwd,
    },
    {
        "cat",
        "Print file contents" /* Concatenate files */,
        b_cat
    },
    {
        "curs",
        "Cursor demo",
        b_curs,
    },
    {
        "sleep",
        "Sleep N seconds",
        b_sleep
    },
    {
        "help",
        "Please help me",
        b_help
    },
    { NULL, NULL, NULL }
};

static int b_ls(const char *arg) {
    int res;
    DIR *dir;
    struct dirent *ent;
    char c;

    if (!arg) {
        arg = "";
    }

    if (!(dir = opendir(arg))) {
        perror(arg);
        return -1;
    }

    while ((ent = readdir(dir))) {
        if (ent->d_name[0] != '.') {
            switch (ent->d_type) {
            case DT_REG:
                c = '-';
                break;
            case DT_DIR:
                c = 'd';
                break;
            case DT_BLK:
                c = 'b';
                break;
            case DT_CHR:
                c = 'c';
                break;
            default:
                c = '?';
                break;
            }

            printf("%c %s\n", c, ent->d_name);
        }
    }

    closedir(dir);

    return 0;
}

static int b_cd(const char *arg) {
    int res;
    if (!arg) {
        arg = "";
    }

    if ((res = chdir(arg)) < 0) {
        perror(arg);
    }

    return res;
}

static int b_pwd(const char *arg) {
    char buf[512];
    if (!getcwd(buf, sizeof(buf))) {
        perror("getcwd()");
        return -1;
    } else {
        puts(buf);
    }
    return 0;
}

static int b_cat(const char *arg) {
    char buf[512];
    int fd;
    ssize_t bread;

    if (!arg) {
        return -1;
    }

    if ((fd = open(arg, O_RDONLY, 0)) < 0) {
        perror(arg);
        return fd;
    }

    while ((bread = read(fd, buf, sizeof(buf))) > 0) {
        write(STDOUT_FILENO, buf, bread);
    }

    close(fd);

    return 0;
}

static int b_curs(const char *arg) {
    char c;

    clear();

    while (1) {
        // Draw some kind of status bar
        curs_set(25, 1);
        printf("\033[7m This is statusbar\033[K\033[0m");
        curs_set(1, 1);

        if (read(STDIN_FILENO, &c, 1) < 0) {
            break;
        }

        if (c == 'q') {
            break;
        }
    }

    curs_set(1, 1);

    return 0;
}

static int b_sleep(const char *arg) {
    if (!arg) {
        return -1;
    }
    int seconds = 0;
    while (*arg) {
        seconds *= 10;
        seconds += *arg - '0';
        ++arg;
    }

    struct timespec ts = { seconds, 0 };
    if ((seconds = nanosleep(&ts, NULL))) {
        perror("nanosleep()");
        return seconds;
    }

    return 0;
}

static int b_help(const char *arg) {
    if (arg) {
        // Describe a specific command
        for (size_t i = 0; builtins[i].run; ++i) {
            if (!strcmp(arg, builtins[i].name)) {
                printf("%s: %s\n", builtins[i].name, builtins[i].desc);
                return 0;
            }
        }

        printf("%s: Unknown command\n", arg);
        return -1;
    } else {
        for (size_t i = 0; builtins[i].run; ++i) {
            printf("%s: %s\n", builtins[i].name, builtins[i].desc);
        }

        return 0;
    }
}

static void prompt(void) {
    printf("\033[36mygg\033[0m > ");
}

static int cmd_exec(const char *line) {
    char cmd[64];
    const char *e = strchr(line, ' ');

    if (!e) {
        assert(strlen(line) < 64);
        strcpy(cmd, line);
    } else {
        assert(e - line < 64);
        strncpy(cmd, line, e - line);
        cmd[e - line] = 0;
        ++e;
        while (*e == ' ') {
            ++e;
        }
    }

    if (e && !*e) {
        e = NULL;
    }

    for (size_t i = 0; builtins[i].run; ++i) {
        if (!strcmp(cmd, builtins[i].name)) {
            return builtins[i].run(e);
        }
    }

    printf("%s: Unknown command\n", cmd);
    return -1;
}

int main(int argc, char **argv) {
    char linebuf[512];
    char c;
    size_t l = 0;
    int res;

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
            if ((res = cmd_exec(linebuf)) != 0) {
                printf("\033[31m= %d\033[0m\n", res);
            }
            prompt();
            continue;
        }

        linebuf[l++] = c;
        write(STDOUT_FILENO, &c, 1);
    }

    return -1;
}
