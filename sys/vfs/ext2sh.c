#include <unistd.h>
#include <assert.h>
#include <stdio.h>
#include "vfs.h"
#include "ext2.h"
#include "testblk.h"

#include <string.h>
#include <ctype.h>
#include <errno.h>

static struct vfs_ioctx ioctx;

static const char *errno_str(int r) {
    switch (r) {
        case -EIO:
            return "I/O error";
        case -ENOENT:
            return "No such file or directory";
        case -EINVAL:
            return "Invalid argument";
        case -EROFS:
            return "Read-only filesystem";
        case -ENOTDIR:
            return "Not a directory";
        case -EISDIR:
            return "Is a directory";
        case -EEXIST:
            return "File exists";
        case -ENOSPC:
            return "No space left on device";
        case -EACCES:
            return "Permission denied";
        default:
            return "Unknown error";
    }
}

static void prettysz(char *out, size_t sz) {
    static const char sizs[] = "KMGTPE???";
    size_t f = sz, r = 0;
    int pwr = 0;
    size_t l = 0;

    while (f >= 1536) {
        r = ((f % 1024) * 10) / 1024;
        f /= 1024;
        ++pwr;
    }

    sprintf(out, "%zu", f);
    l = strlen(out);

    if (pwr) {
        out[l++] = '.';
        out[l++] = '0' + r;

        out[l++] = sizs[pwr - 1];

        out[l++] = 'i';
    }

    out[l++] = 'B';
    out[l++] = 0;
}

static void dumpstat(char *buf, const struct stat *st) {
    char t = '-';

    switch (st->st_mode & S_IFMT) {
    case S_IFDIR:
        t = 'd';
        break;
    case S_IFLNK:
        t = 'l';
        break;
    }

    sprintf(buf, "%c%c%c%c%c%c%c%c%c%c % 5d % 5d %u %u", t,
        (st->st_mode & S_IRUSR) ? 'r' : '-',
        (st->st_mode & S_IWUSR) ? 'w' : '-',
        (st->st_mode & S_IXUSR) ? 'x' : '-',
        (st->st_mode & S_IRGRP) ? 'r' : '-',
        (st->st_mode & S_IWGRP) ? 'w' : '-',
        (st->st_mode & S_IXGRP) ? 'x' : '-',
        (st->st_mode & S_IROTH) ? 'r' : '-',
        (st->st_mode & S_IWOTH) ? 'w' : '-',
        (st->st_mode & S_IXOTH) ? 'x' : '-',
        st->st_uid,
        st->st_gid,
        st->st_size,
        st->st_ino);
}

static int shell_stat(const char *path) {
    struct stat st;
    int res = vfs_stat(&ioctx, path, &st);

    if (res == 0) {
        char buf[64];
        char linkp[1024];
        dumpstat(buf, &st);

        if ((st.st_mode & S_IFMT) == S_IFLNK) {
            // Try to read the link
            if ((res = vfs_readlink(&ioctx, path, linkp)) < 0) {
                return res;
            }

            printf("%s\t%s -> %s\n", buf, path, linkp);
        } else {
            printf("%s\t%s\n", buf, path);
        }
    }

    return res;
}

static int shell_tree(const char *arg) {
    vfs_dump_tree();
    return 0;
}

static int shell_ls(const char *arg) {
    struct ofile fd;
    int res = vfs_open(&ioctx, &fd, arg, 0, O_DIRECTORY | O_RDONLY);
    if (res < 0) {
        return res;
    }

    struct dirent *ent;
    while ((ent = vfs_readdir(&ioctx, &fd))) {
        printf("dirent %s\n", ent->d_name);
    }

    vfs_close(&ioctx, &fd);

    return res;
}

static int shell_ls_detail(const char *arg) {
    struct ofile fd;
    struct dirent *ent;
    struct stat st;
    char fullp[512];
    char linkp[1024];
    int res;

    if ((res = vfs_open(&ioctx, &fd, arg, 0, O_DIRECTORY | O_RDONLY)) < 0) {
        return res;
    }

    while ((ent = vfs_readdir(&ioctx, &fd))) {
        if (ent->d_name[0] == '.') {
            continue;
        }

        if ((res = vfs_statat(&ioctx, fd.vnode, ent->d_name, &st)) < 0) {
            return res;
        }

        dumpstat(fullp, &st);

        if ((st.st_mode & S_IFMT) == S_IFLNK) {
            // Try to read the link
            if ((res = vfs_readlinkat(&ioctx, fd.vnode, ent->d_name, linkp)) < 0) {
                return res;
            }

            printf("%s\t%s -> %s\n", fullp, ent->d_name, linkp);
        } else {
            printf("%s\t%s\n", fullp, ent->d_name);
        }
    }

    vfs_close(&ioctx, &fd);
    return 0;
}

static int shell_cat(const char *arg) {
    struct ofile fd;
    int res;
    char buf[512];
    size_t bread = 0;

    if ((res = vfs_open(&ioctx, &fd, arg, 0, O_RDONLY)) != 0) {
        return res;
    }

    while ((res = vfs_read(&ioctx, &fd, buf, sizeof(buf))) > 0) {
        fwrite(buf, 1, res, stdout);
        bread += res;
    }

    vfs_close(&ioctx, &fd);

    printf("\n%zuB total\n", bread);
    return 0;
}

static int shell_setcwd(const char *arg) {
    return vfs_setcwd(&ioctx, arg);
}

static int shell_cd(const char *arg) {
    return vfs_chdir(&ioctx, arg);
}

static int shell_touch(const char *arg) {
    int res;
    struct ofile fd;

    if ((res = vfs_creat(&ioctx, &fd, arg, 0644, O_RDWR)) != 0) {
        return res;
    }

    vfs_close(&ioctx, &fd);

    return 0;
}

static int shell_hello(const char *arg) {
    struct ofile fd;
    int res;
    char linebuf[512];

    printf("= ");
    if (!fgets(linebuf, sizeof(linebuf), stdin)) {
        return -1;
    }

    if ((res = vfs_open(&ioctx, &fd, arg, 0644, O_TRUNC | O_CREAT | O_WRONLY)) < 0) {
        return res;
    }

    if ((res = vfs_write(&ioctx, &fd, linebuf, strlen(linebuf))) < 0) {
        return res;
    }

    vfs_close(&ioctx, &fd);

    return 0;
}

static int shell_trunc(const char *arg) {
    struct ofile fd;
    int res;

    // TODO: implement O_TRUNC
    if ((res = vfs_open(&ioctx, &fd, arg, 0644, O_WRONLY)) < 0) {
        return res;
    }

    if ((res = vfs_truncate(&ioctx, &fd, 0)) < 0) {
        return res;
    }

    vfs_close(&ioctx, &fd);

    return 0;
}

static int shell_unlink(const char *arg) {
    return vfs_unlink(&ioctx, arg);
}

static int shell_mkdir(const char *arg) {
    return vfs_mkdir(&ioctx, arg, 0755);
}

static int shell_chmod(const char *arg) {
    char buf[64];
    mode_t mode;

    printf("mode = ");
    if (!fgets(buf, sizeof(buf), stdin)) {
        return -1;
    }
    sscanf(buf, "%o", &mode);
    return vfs_chmod(&ioctx, arg, mode);
}

static int shell_chown(const char *arg) {
    char buf[64];
    uid_t uid;
    gid_t gid;

    printf("uid = ");
    if (!fgets(buf, sizeof(buf), stdin)) {
        return -1;
    }
    sscanf(buf, "%d", &uid);
    printf("gid = ");
    if (!fgets(buf, sizeof(buf), stdin)) {
        return -1;
    }
    sscanf(buf, "%d", &gid);

    return vfs_chown(&ioctx, arg, uid, gid);
}

static int shell_access(const char *arg) {
    char buf[64];
    const char *p;
    int mode = 0;

    printf("rwx = ");
    if (!(p = fgets(buf, sizeof(buf), stdin))) {
        return -1;
    }

    while (*p) {
        switch (*p++) {
        case 'r':
            mode |= R_OK;
            break;
        case 'w':
            mode |= W_OK;
            break;
        case 'x':
            mode |= X_OK;
            break;
        case '\n':
            break;
        default:
            fprintf(stderr, "Unknown access flag: %c\n", *(p - 1));
            return -1;
        }
    }

    return vfs_access(&ioctx, arg, mode);
}

static int shell_setuid(const char *arg) {
    uid_t uid;
    sscanf(arg, "%d", &uid);

    ioctx.uid = uid;

    return 0;
}

static int shell_setgid(const char *arg) {
    gid_t gid;
    sscanf(arg, "%d", &gid);

    ioctx.gid = gid;

    return 0;
}

static int shell_me(const char *arg) {
    printf("me: uid = %d, gid = %d\n", ioctx.uid, ioctx.gid);
    return 0;
}

static int shell_df(const char *arg) {
    struct statvfs st;
    int res;

    if ((res = vfs_statvfs(&ioctx, arg, &st)) < 0) {
        return res;
    }

    char buf[24];

    printf("%12s %12s %12s %5s\n",
            "Blocks",
            "Used",
            "Free",
            "Use%");
    printf("% 12zd % 12zd % 12zd % 4zd%%\n\n",
           st.f_blocks,
           st.f_blocks - st.f_bfree,
           st.f_bfree,
           ((st.f_blocks - st.f_bfree) * 100) / st.f_blocks);
    printf("%12s %12s %12s %5s\n",
            "Total",
            "Used",
            "Free",
            "Use%");

    prettysz(buf, st.f_blocks * st.f_bsize);
    printf("%12s ", buf);
    prettysz(buf, (st.f_blocks - st.f_bfree) * st.f_bsize);
    printf("%12s ", buf);
    prettysz(buf, st.f_bfree * st.f_bsize);
    printf("%12s % 4zd%%\n", buf,
           ((st.f_blocks - st.f_bfree) * 100) / st.f_blocks);

    return 0;
}

static int shell_readlink(const char *arg) {
    int res;
    char buf[1024];

    if ((res = vfs_readlink(&ioctx, arg, buf)) < 0) {
        return res;
    }

    printf("%s\n", buf);

    return 0;
}

static int shell_symlink(const char *arg) {
    int res;
    char buf[1024];

    printf("dst = ");

    if (!fgets(buf, sizeof(buf), stdin)) {
        return -1;
    }

    size_t l = strlen(buf);
    while (l && (!buf[l] || buf[l] == '\n')) {
        buf[l--] = 0;
    }

    return vfs_symlink(&ioctx, buf, arg);
}

static struct {
    const char *name;
    int (*fn) (const char *arg);
} shell_cmds[] = {
    { "stat", shell_stat },
    { "tree", shell_tree },
    { "cat", shell_cat },
    { "ll", shell_ls_detail },
    { "ls", shell_ls },
    { "setcwd", shell_setcwd },
    { "touch", shell_touch },
    { "hello", shell_hello },
    { "trunc", shell_trunc },
    { "unlink", shell_unlink },
    { "mkdir", shell_mkdir },
    { "chmod", shell_chmod },
    { "chown", shell_chown },
    { "access", shell_access },
    { "setuid", shell_setuid },
    { "setgid", shell_setgid },
    { "readlink", shell_readlink },
    { "symlink", shell_symlink },
    { "df", shell_df },
    { "me", shell_me },
    { "cd", shell_cd },
};

static void shell(void) {
    char linebuf[256];
    char namebuf[64];
    const char *cmd;
    const char *arg;

    while (1) {
        fputs("> ", stdout);
        if (!fgets(linebuf, sizeof(linebuf), stdin)) {
            break;
        }

        size_t i = strlen(linebuf);
        if (i == 0) {
            continue;
        }
        --i;
        while (isspace(linebuf[i])) {
            linebuf[i] = 0;
            --i;
        }

        const char *p = strchr(linebuf, ' ');
        if (p) {
            strncpy(namebuf, linebuf, p - linebuf);
            namebuf[p - linebuf] = 0;
            cmd = namebuf;
            arg = p + 1;
        } else {
            cmd = linebuf;
            arg = "";
        }

        for (int i = 0; i < sizeof(shell_cmds) / sizeof(shell_cmds[0]); ++i) {
            if (!strcmp(shell_cmds[i].name, cmd)) {
                int res = shell_cmds[i].fn(arg);
                if (res != 0) {
                    fprintf(stderr, "%s: %s\n", linebuf, errno_str(res));
                }

                goto found;
            }
        }

        fprintf(stderr, "Command not found: %s\n", cmd);
found:
        continue;
    }
}

int main(int argc, const char **argv) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <image-file>\n", argv[0]);
        return -1;
    }

    if (access(argv[1], O_RDONLY) < 0) {
        perror(argv[1]);
        return -1;
    }

    int res;

    ioctx.uid = 0;
    ioctx.gid = 0;
    ioctx.cwd_vnode = NULL;

    vfs_init();
    ext2_class_init();
    testblk_init(argv[1]);

    // Mount ext2 as rootfs
    if ((res = vfs_mount(&ioctx, "/", &testblk_dev, "ext2", NULL)) != 0) {
        fprintf(stderr, "Failed to mount rootfs\n");
        return -1;
    }

    ioctx.uid = 1000;
    ioctx.gid = 1000;

    shell();

    // Unmount as root
    ioctx.gid = 0;
    ioctx.uid = 0;
    // Cleanup
    if ((res = vfs_umount(&ioctx, "/")) != 0) {
        fprintf(stderr, "Failed to umount /: %s\n", errno_str(res));
        return -1;
    }
    testblk_dev.destroy(&testblk_dev);

    return 0;
}
