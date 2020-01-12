/** vi: ft=c.doxygen :
 * @file vfs.h
 * @brief Virtual filesystem (VFS) core functions
 */
#pragma once
#include "tree.h"
#include "node.h"
#include "ofile.h"
#include "sys/stat.h"
#include "sys/statvfs.h"

#define PATH_MAX        256
#define LINK_MAX        16

struct vfs_ioctx {
    // Process' current working directory
    char cwd_path[PATH_MAX];
    struct vnode *cwd_vnode;
    uid_t uid;
    gid_t gid;
};

extern struct vfs_ioctx *const kernel_ioctx;

void vfs_init(void);

int vfs_link_resolve(struct vfs_ioctx *ctx, struct vnode *lnk, struct vnode **res);
int vfs_find(struct vfs_ioctx *ctx, struct vnode *rel, const char *path, struct vnode **node);
int vfs_mount(struct vfs_ioctx *ctx, const char *at, void *blk, const char *fs, const char *opt);

int vfs_open(struct vfs_ioctx *ctx, struct ofile *fd, const char *path, int flags, int mode);
void vfs_close(struct vfs_ioctx *ctx, struct ofile *fd);

int vfs_stat(struct vfs_ioctx *ctx, const char *path, struct stat *st);

ssize_t vfs_read(struct vfs_ioctx *ctx, struct ofile *fd, void *buf, size_t count);
