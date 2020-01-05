#include <sys/stat.h>
#include <dirent.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <stdio.h>

#define LS_COLOR        (1 << 0)
#define LS_DETAIL       (1 << 1)

#define COLOR_DIR       "\033[36m"
#define COLOR_DEV       "\033[33m"
#define COLOR_UNKNOWN   "\033[43m"

#define COLOR_RESET     "\033[0m"

static int ls_dir(const char *path, int flags) {
    int res;
    DIR *dir;
    struct dirent *ent;
    struct stat ent_stat;
    char ent_path[512];
    char t;

    if (!(dir = opendir(path))) {
        perror(path);
        return -1;
    }

    while ((ent = readdir(dir))) {
        if (ent->d_name[0] != '.') {
            if (flags & LS_DETAIL) {
                snprintf(ent_path, sizeof(ent_path), "%s/%s",
                    strcmp(path, "") ? path : ".", ent->d_name);
                if (stat(ent_path, &ent_stat) != 0) {
                    printf("??????????    ?    ?        ? ");
                } else {
                    switch (ent_stat.st_mode & S_IFMT) {
                    case S_IFDIR:
                        t = 'd';
                        break;
                    case S_IFREG:
                        t = '-';
                        break;
                    case S_IFBLK:
                        t = 'b';
                        break;
                    case S_IFCHR:
                        t = 'c';
                        break;
                    default:
                        t = '?';
                        break;
                    }

                    printf("%c%c%c%c%c%c%c%c%c%c ",
                        t,
                        (ent_stat.st_mode & S_IRUSR) ? 'r' : '-',
                        (ent_stat.st_mode & S_IWUSR) ? 'w' : '-',
                        (ent_stat.st_mode & S_IXUSR) ? 'x' : '-',
                        (ent_stat.st_mode & S_IRGRP) ? 'r' : '-',
                        (ent_stat.st_mode & S_IWGRP) ? 'w' : '-',
                        (ent_stat.st_mode & S_IXGRP) ? 'x' : '-',
                        (ent_stat.st_mode & S_IROTH) ? 'r' : '-',
                        (ent_stat.st_mode & S_IWOTH) ? 'w' : '-',
                        (ent_stat.st_mode & S_IXOTH) ? 'x' : '-');

                    printf("%4u %4u %8u ",
                        ent_stat.st_gid,
                        ent_stat.st_uid,
                        ent_stat.st_blocks);
                }
            }

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
            break;
        } else {
            if (!strcmp(argv[i], "-c")) {
                flags |= LS_COLOR;
            } else if (!strcmp(argv[i], "-d")) {
                flags |= LS_DETAIL;
            } else {
                printf("Unknown option: %s\n", argv[i]);
            }
        }
    }

    if (first == -1) {
        ls_dir("", flags);
    } else {
        for (int i = first; i < argc; ++i) {
            if (first != argc - 1) {
                printf("%s:\n", argv[i]);
            }
            ls_dir(argv[i], flags);
        }
    }

    return 0;
}
