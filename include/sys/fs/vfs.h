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

/**
 * @brief Internal VFS tree node.
 *        As the actual vnodes are not associated
 *        with any location in the filesystem tree,
 *        we use this struct to keep things in order.
 */
struct vfs_node {
    char name[256];
    // Current vnode unless mnt, otherwise the root node of
    // the mounted filesystem
    vnode_t *vnode;
    // Real vnode if mountpoint
    vnode_t *real_vnode;
    // Is 1 when node is a mountpoint
    int ismount;
    // Parent ref
    struct vfs_node *parent;
    // Linked list of children
    struct vfs_node *child;
    struct vfs_node *cdr;
};

/**
 * @brief Process-specific I/O context details
 * @note This will be moved to other headers once
 *       I integrate the VFS project into my kernel.
 */
struct vfs_ioctx {
    // Process' current working directory
    vnode_t *cwd_vnode;
    uid_t uid;
    gid_t gid;
};

/**
 * @brief Set process' current working directory
 * @param ctx I/O context
 * @param cwd New CWD
 * @return 0 on success, negative values on error
 */
int vfs_setcwd(struct vfs_ioctx *ctx, const char *cwd);
/**
 * @brief Set process' working directory relative to the
 *        current one.
 * @param ctx I/O context
 * @param cwd_rel Relative/absolute path
 * @return 0 on success, negative values on error
 */
int vfs_chdir(struct vfs_ioctx *ctx, const char *cwd_rel);


/**
 * @brief Initialize the VFS
 */
void vfs_init(void);
/**
 * @brief Dump the internal filesystem representation
 */
void vfs_dump_tree(void);
/**
 * @brief Get a string representation of a path
 *        inside a filesystem tree.
 * @param path The result path
 * @param vn vnode
 */
void vfs_vnode_path(char *path, vnode_t *vn);

// Tree node ops
/**
 * @brief Free internal VFS node struct
 * @param n Tree node
 */
void vfs_node_free(struct vfs_node *n);
/**
 * @brief Allocate a new node and associate it with
 *        a vnode
 * @param name Name to be given to the tree node
 * @param vn The vnode to bind it to
 * @return Non-NULL (struct vfs_node *) on success, NULL otherwise
 */
struct vfs_node *vfs_node_create(const char *name, vnode_t *vn);

int vfs_mount_internal(struct vfs_node *at, void *blkdev, const char *fs_name, const char *opt);
/**
 * @brief Create a mountpoint in a filesystem tree so that
 *        a "source" directory actually refers to the root
 *        node of a "target" filesystem.
 * @param ctx I/O context
 * @param at Path to create mountpoint at
 * @param blk Block device on which a filesystem shall be mounted
 * @param fs_name Filesystem name
 * @param fs_opt Filesystem mounting options
 * @return 0 on success,
 *         -EACCES if the caller context's UID is not zero,
 *         -EBUSY if trying to create a mountpoint at a
 *                directory which is not empty,
 *         -ENOENT if the directory does not exist
 */
int vfs_mount(struct vfs_ioctx *ctx, const char *at, void *blk, const char *fs_name, const char *fs_opt);
/**
 * @brief Destroy a mountpoint in a filesystem tree
 * @param ctx I/O context
 * @param target Path to unmount
 * @return 0 on success,
 *         -EBUSY if there are any used filesystem objects inside
 *                the mountpoint,
 *         -EACCES if the caller context's UID is not zero,
 *         -ENOENT if the directory does not exist
 */
int vfs_umount(struct vfs_ioctx *ctx, const char *target);

/**
 * @brief Read symlink's destination path into a buffer
 * @param ctx I/O context
 * @param path
 */
int vfs_readlink(struct vfs_ioctx *ctx, const char *path, char *buf);
int vfs_readlinkat(struct vfs_ioctx *ctx, vnode_t *at, const char *path, char *buf);
int vfs_symlink(struct vfs_ioctx *ctx, const char *target, const char *linkpath);

// File ops
int vfs_truncate(struct vfs_ioctx *ctx, struct ofile *fd, size_t length);
int vfs_creat(struct vfs_ioctx *ctx, struct ofile *fd, const char *path, int mode, int opt);
int vfs_open(struct vfs_ioctx *ctx, struct ofile *fd, const char *path, int mode, int opt);
int vfs_open_node(struct vfs_ioctx *ctx, struct ofile *fd, vnode_t *vn, int opt);
void vfs_close(struct vfs_ioctx *ctx, struct ofile *fd);
ssize_t vfs_read(struct vfs_ioctx *ctx, struct ofile *fd, void *buf, size_t count);
ssize_t vfs_write(struct vfs_ioctx *ctx, struct ofile *fd, const void *buf, size_t count);
int vfs_unlink(struct vfs_ioctx *ctx, const char *path, int is_rmdir);

int vfs_stat(struct vfs_ioctx *ctx, const char *path, struct stat *st);
int vfs_statat(struct vfs_ioctx *ctx, vnode_t *at, const char *path, struct stat *st);
int vfs_chmod(struct vfs_ioctx *ctx, const char *path, mode_t mode);
int vfs_chown(struct vfs_ioctx *ctx, const char *path, uid_t uid, gid_t gid);
int vfs_access(struct vfs_ioctx *ctx, const char *path, int mode);
// Directroy ops
int vfs_mkdir(struct vfs_ioctx *ctx, const char *path, mode_t mode);
struct dirent *vfs_readdir(struct vfs_ioctx *ctx, struct ofile *fd);

int vfs_statvfs(struct vfs_ioctx *ctx, const char *path, struct statvfs *st);
