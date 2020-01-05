#include <dirent.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <stdio.h>

#define LS_COLOR        (1 << 0)

#define COLOR_DIR       "\033[36m"
#define COLOR_DEV       "\033[33m"
#define COLOR_UNKNOWN   "\033[43m"

#define COLOR_RESET     "\033[0m"

static int ls_dir(const char *path, int flags) {
    int res;
    DIR *dir;
    struct dirent *ent;

    if (!(dir = opendir(path))) {
        perror(path);
        return -1;
    }

    while ((ent = readdir(dir))) {
        if (ent->d_name[0] != '.') {
            if (flags & LS_COLOR) {
                switch (ent->d_type) {
                case DT_REG:
                    break;
                case DT_BLK:
                case DT_CHR:
                    write(STDOUT_FILENO, COLOR_DEV, 5);
                    break;
                case DT_DIR:
                    write(STDOUT_FILENO, COLOR_DIR, 5);
                    break;
                default:
                    write(STDOUT_FILENO, COLOR_UNKNOWN, 5);
                    break;
                }
            }
            write(STDOUT_FILENO, ent->d_name, strlen(ent->d_name));
            if (flags & LS_COLOR) {
                write(STDOUT_FILENO, COLOR_RESET, 4);
            }
            putchar('\n');
        }
    }

    closedir(dir);

    return 0;
}

int main(int argc, char **argv) {
    // Basic getopt-like features
    int first = -1;
    int flags = 0;

    for (int i = 1; i < argc; ++i) {
        if (argv[i][0] != '-') {
            first = i;
        } else {
            if (!strcmp(argv[i], "-c")) {
                flags |= LS_COLOR;
            }
        }
    }

    if (first == -1) {
        ls_dir("", flags);
    } else {
        for (int i = first; i < argc; ++i) {
            ls_dir(argv[i], flags);
        }
    }

    return 0;
}
