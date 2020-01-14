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
    struct vnode *cwd_vnode;
    uid_t uid;
    gid_t gid;
};

extern struct vfs_ioctx *const kernel_ioctx;

mode_t vfs_vnode_to_mode(enum vnode_type type);

void vfs_init(void);

int vfs_setcwd(struct vfs_ioctx *ctx, const char *rel_path);
void vfs_vnode_path(char *path, struct vnode *node);

int vfs_link_resolve(struct vfs_ioctx *ctx, struct vnode *lnk, struct vnode **res);
int vfs_find(struct vfs_ioctx *ctx, struct vnode *rel, const char *path, struct vnode **node);
int vfs_mount(struct vfs_ioctx *ctx, const char *at, void *blk, const char *fs, const char *opt);

int vfs_open_vnode(struct vfs_ioctx *ctx, struct ofile *fd, struct vnode *node, int opt);
int vfs_open(struct vfs_ioctx *ctx, struct ofile *fd, const char *path, int flags, int mode);
void vfs_close(struct vfs_ioctx *ctx, struct ofile *fd);
int vfs_readdir(struct vfs_ioctx *ctx, struct ofile *fd, struct dirent *ent);

int vfs_access(struct vfs_ioctx *ctx, const char *path, int accmode);
int vfs_stat(struct vfs_ioctx *ctx, const char *path, struct stat *st);
int vfs_access_check(struct vfs_ioctx *ctx, int desm, mode_t mode, uid_t uid, gid_t gid);
int vfs_access_node(struct vfs_ioctx *ctx, struct vnode *vn, int mode);

ssize_t vfs_write(struct vfs_ioctx *ctx, struct ofile *fd, const void *buf, size_t count);
ssize_t vfs_read(struct vfs_ioctx *ctx, struct ofile *fd, void *buf, size_t count);

off_t vfs_lseek(struct vfs_ioctx *ctx, struct ofile *fd, off_t offset, int whence);
