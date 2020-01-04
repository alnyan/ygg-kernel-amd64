#include <sys/fcntl.h>
#include <sys/reboot.h>
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

#define BOX_ANGLE_UL    "\332"
#define BOX_ANGLE_UR    "\277"
#define BOX_ANGLE_LL    "\300"
#define BOX_ANGLE_LR    "\331"
#define BOX_HOR         "\304"
#define BOX_VERT        "\263"

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
static int b_clear(const char *arg);
static int b_reboot(const char *arg);

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
        "clear",
        "Clear terminal",
        b_clear,
    },
    {
        "reboot",
        "Reboot",
        b_reboot
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

    size_t w = 60;
    size_t h = 20;
    size_t off_y = (25 - h) / 2 + 1;
    size_t off_x = (80 - w) / 2;

    const char *lines[18] = {
        NULL,
        "Demo something",
        NULL,
        "Slow as hell"
    };

    clear();

    while (1) {
        printf("\033[47;30m");

        curs_set(off_y, off_x);
        printf(BOX_ANGLE_UL);
        for (size_t i = 0; i < w; ++i) {
            printf(BOX_HOR);
        }
        printf(BOX_ANGLE_UR);

        for (size_t i = 0; i < h - 2; ++i) {
            curs_set(off_y + 1 + i, off_x);
            printf(BOX_VERT);
            for (size_t j = 0; j < w; ++j) {
                printf(" ");
            }
            if (lines[i]) {
                curs_set(off_y + 1 + i, off_x + (w - strlen(lines[i])) / 2);
                printf("%s", lines[i]);
            }
            curs_set(off_y + 1 + i, off_x + w + 1);
            printf(BOX_VERT);
        }

        curs_set(off_y + h - 3, off_x + (w - 8) / 2);
        printf("\033[0m\033[7m[ OK ]\033[47;30m");
        curs_set(off_y + h - 1, off_x);
        printf(BOX_ANGLE_LL);
        for (size_t i = 0; i < w; ++i) {
            printf(BOX_HOR);
        }
        printf(BOX_ANGLE_LR);

        printf("\033[0m");

        curs_set(1, 1);

        if (read(STDIN_FILENO, &c, 1) < 0) {
            break;
        }

        if (c == 'q' || c == '\n') {
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

static int b_clear(const char *arg) {
    clear();
    curs_set(1, 1);
    return 0;
}

static int b_reboot(const char *arg) {
    int res;
    unsigned int cmd = YGG_REBOOT_RESTART;

    if (arg) {
        if (!strcmp(arg, "-s")) {
            cmd = YGG_REBOOT_POWER_OFF;
        } else if (!strcmp(arg, "-h")) {
            cmd = YGG_REBOOT_HALT;
        }
    }

    if ((res = reboot(YGG_REBOOT_MAGIC1, YGG_REBOOT_MAGIC2, cmd, NULL)) < 0) {
        perror("reboot()");
        return res;
    }

    while (1) {
        usleep(1000000);
    }
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

    // Try to execute binary from /
    struct stat st;
    char path[64] = "/";
    strcat(path, cmd);
    if (stat(path, &st) == 0) {
        const char *argp[] = {
            e, NULL
        };

        int pid = fork();

        switch (pid) {
        case -1:
            perror("fork()");
            return -1;
        case 0:
            if (execve(path, argp, NULL) != 0) {
                perror("execve()");
            }
            exit(-1);
        default:
            // TODO: waitpid
            return 0;
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

#if 0
    if ((res = fork()) < 0) {
        perror("fork()");
        return -1;
    } else if (res == 0) {
        if (execve("/time", NULL, NULL) != 0) {
            perror("execve()");
        }
        exit(-1);
    }
#endif

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
