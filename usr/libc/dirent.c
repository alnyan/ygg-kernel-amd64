#include "dirent.h"
#include "bits/syscall.h"
#include <sys/fcntl.h>
#include <string.h>
#include <errno.h>

extern void *malloc(size_t count);
extern void free(void *p);

struct DIR_private {
    int fd;
    struct dirent buf;
};

DIR *opendir(const char *path) {
    int fd;
    DIR *res;

    if (!(res = malloc(sizeof(struct DIR_private)))) {
        errno = ENOMEM;
        return NULL;
    }


    if ((fd = open(path, O_DIRECTORY | O_RDONLY, 0)) < 0) {
        fd = errno;
        free(res);
        errno = fd;
        // errno is set
        return NULL;
    }

    res->fd = fd;
    memset(&res->buf, 0, sizeof(res->buf));

    return res;
}

void closedir(DIR *dirp) {
    if (!dirp) {
        return;
    }
    close(dirp->fd);
    free(dirp);
}

struct dirent *readdir(DIR *dirp) {
    ssize_t res;
    if (!dirp) {
        errno = EBADF;
        return NULL;
    }
    if ((res = sys_readdir(dirp->fd, &dirp->buf)) <= 0) {
        return NULL;
    }
    return &dirp->buf;
}
