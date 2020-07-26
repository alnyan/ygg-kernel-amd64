/** vi: ft=c.doxygen :
 * @file vfs.h
 * @brief Virtual filesystem (VFS) core functions
 */
#pragma once
#include "sys/types.h"

#define PATH_MAX        256
#define LINK_MAX        16

#define VFS_MODE_MASK   0xFFF

struct fs_class;
enum vnode_type;
struct dirent;
struct ofile;
struct vnode;
struct stat;

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

int vfs_mount_internal(struct vnode *at, void *blk, struct fs_class *cls, uint32_t flags, const char *opt);

int vfs_link_resolve(struct vfs_ioctx *ctx, struct vnode *lnk, struct vnode **res, int link_itself);
int vfs_find(struct vfs_ioctx *ctx, struct vnode *rel, const char *path, int link_itself, struct vnode **node);
int vfs_mount(struct vfs_ioctx *ctx, const char *at, void *blk, const char *fs, uint32_t flags, const char *opt);
int vfs_umount(struct vfs_ioctx *ctx, const char *dir_name);

int vfs_open_vnode(struct vfs_ioctx *ctx, struct ofile *fd, struct vnode *node, int opt);
int vfs_openat(struct vfs_ioctx *ctx,
               struct ofile *fd,
               struct vnode *at,
               const char *path,
               int flags, int mode);
void vfs_close(struct vfs_ioctx *ctx, struct ofile *fd);
int vfs_readdir(struct vfs_ioctx *ctx, struct ofile *fd, struct dirent *ent);
int vfs_unlinkat(struct vfs_ioctx *ctx, struct vnode *at, const char *pathname, int flags);
int vfs_mkdirat(struct vfs_ioctx *ctx, struct vnode *at, const char *path, mode_t mode);
int vfs_mknod(struct vfs_ioctx *ctx, const char *path, mode_t mode, struct vnode **nod);
int vfs_chmod(struct vfs_ioctx *ctx, const char *path, mode_t mode);
int vfs_chown(struct vfs_ioctx *ctx, const char *path, uid_t uid, gid_t gid);
int vfs_ioctl(struct vfs_ioctx *ctx, struct ofile *fd, unsigned int cmd, void *arg);
int vfs_ftruncate(struct vfs_ioctx *ctx, struct vnode *node, off_t length);

int vfs_faccessat(struct vfs_ioctx *ctx, struct vnode *at, const char *path, int accmode, int flags);
int vfs_fstatat(struct vfs_ioctx *ctx, struct vnode *at, const char *path, struct stat *st, int flags);
int vfs_access_check(struct vfs_ioctx *ctx, int desm, mode_t mode, uid_t uid, gid_t gid);
int vfs_access_node(struct vfs_ioctx *ctx, struct vnode *vn, int mode);

ssize_t vfs_write(struct vfs_ioctx *ctx, struct ofile *fd, const void *buf, size_t count);
ssize_t vfs_read(struct vfs_ioctx *ctx, struct ofile *fd, void *buf, size_t count);

off_t vfs_lseek(struct vfs_ioctx *ctx, struct ofile *fd, off_t offset, int whence);
