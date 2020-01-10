#include "sys/fs/vfs.h"
#include "sys/fcntl.h"
#include "sys/assert.h"
#include "sys/string.h"
#include "sys/debug.h"
#include "sys/fs/fs.h"
#include "sys/errno.h"
#include "sys/panic.h"
#include "sys/heap.h"
#include "sys/blk.h"
#include "sys/chr.h"

// #include <stddef.h>
// #include <string.h>
// #include <_assert.h>
// #include <stdlib.h>
// #include <errno.h>
//
// #include <stdio.h>

//static struct vfs_node vfs_root_node;
//
//static int vfs_find(vnode_t *cwd_vnode, const char *path, vnode_t **res_vnode);
//static int vfs_access_internal(struct vfs_ioctx *ctx, int desm, mode_t mode, uid_t uid, gid_t gid);
//static int vfs_vnode_access(struct vfs_ioctx *ctx, vnode_t *vn, int mode);
//
//static int vfs_setcwd_rel(struct vfs_ioctx *ctx, vnode_t *at, const char *path) {
//    // cwd is absolute path
//    vnode_t *new_cwd;
//    int res;
//    if ((res = vfs_find(at, path, &new_cwd)) != 0) {
//        return res;
//    }
//
//    vnode_ref(new_cwd);
//    if (new_cwd->type != VN_DIR) {
//        vnode_unref(new_cwd);
//        return -ENOTDIR;
//    }
//
//    if ((res = vfs_vnode_access(ctx, new_cwd, X_OK)) < 0) {
//        vnode_unref(new_cwd);
//        return res;
//    }
//
//    if (ctx->cwd_vnode) {
//        vnode_unref(ctx->cwd_vnode);
//    }
//    ctx->cwd_vnode = new_cwd;
//
//    return 0;
//}

//int vfs_setcwd(struct vfs_ioctx *ctx, const char *cwd) {
//    return vfs_setcwd_rel(ctx, NULL, cwd);
//}

//int vfs_chdir(struct vfs_ioctx *ctx, const char *cwd_rel) {
//    return vfs_setcwd_rel(ctx, ctx->cwd_vnode, cwd_rel);
//}

//void vfs_vnode_path(char *path, vnode_t *vn) {
//    size_t c = 0;
//    if (!vn) {
//        path[0] = '/';
//        path[1] = 0;
//        return;
//    }
//    struct vfs_node *node = vn->tree_node;
//    struct vfs_node *backstack[10] = { NULL };
//    if (!node) {
//        panic("Unhandled: NULL node\n");
//    }
//    if (!node->parent) {
//        path[0] = '/';
//        path[1] = 0;
//        return;
//    }
//    size_t bstp = 10;
//    while (node) {
//        if (bstp == 0) {
//            panic("Node stack overflow\n");
//        }
//
//        backstack[--bstp] = node;
//        node = node->parent;
//    }
//
//    for (size_t i = bstp; i < 10; ++i) {
//        size_t len;
//        if (backstack[i]->parent) {
//            // Non-root
//            len = strlen(backstack[i]->name);
//            strcpy(path + c, backstack[i]->name);
//        } else {
//            len = 0;
//        }
//        c += len;
//        if (i != 9) {
//            path[c] = '/';
//        }
//        ++c;
//    }
//}
//
//static int vfs_open_access_mask(int oflags) {
//    if (oflags & O_EXEC) {
//        return X_OK;
//    }
//
//    switch (oflags & O_ACCMODE) {
//    case O_WRONLY:
//        return W_OK;
//    case O_RDONLY:
//        return R_OK;
//    case O_RDWR:
//        return R_OK | W_OK;
//    default:
//        panic("Unknown access mode\n");
//    }
//}
//
//static int vfs_access_internal(struct vfs_ioctx *ctx, int desm, mode_t mode, uid_t uid, gid_t gid) {
//    if (ctx->uid == 0) {
//        if (desm & X_OK) {
//            // Check if anyone at all can execute this
//            if (!(mode & (S_IXOTH | S_IXGRP | S_IXUSR))) {
//                return -EACCES;
//            }
//        }
//
//        return 0;
//    }
//
//    if (uid == ctx->uid) {
//        if ((desm & R_OK) && !(mode & S_IRUSR)) {
//            return -EACCES;
//        }
//        if ((desm & W_OK) && !(mode & S_IWUSR)) {
//            return -EACCES;
//        }
//        if ((desm & X_OK) && !(mode & S_IXUSR)) {
//            return -EACCES;
//        }
//    } else if (gid == ctx->gid) {
//        if ((desm & R_OK) && !(mode & S_IRGRP)) {
//            return -EACCES;
//        }
//        if ((desm & W_OK) && !(mode & S_IWGRP)) {
//            return -EACCES;
//        }
//        if ((desm & X_OK) && !(mode & S_IXGRP)) {
//            return -EACCES;
//        }
//    } else {
//        if ((desm & R_OK) && !(mode & S_IROTH)) {
//            return -EACCES;
//        }
//        if ((desm & W_OK) && !(mode & S_IWOTH)) {
//            return -EACCES;
//        }
//        if ((desm & X_OK) && !(mode & S_IXOTH)) {
//            return -EACCES;
//        }
//    }
//
//    return 0;
//}
//
//static int vfs_vnode_access(struct vfs_ioctx *ctx, vnode_t *vn, int mode) {
//    mode_t vn_mode;
//    uid_t vn_uid;
//    gid_t vn_gid;
//    int res;
//
//    // Filesystem does not have permissions
//    if (!vn->op || !vn->op->access) {
//        return 0;
//    }
//
//    if ((res = vn->op->access(vn, &vn_uid, &vn_gid, &vn_mode)) < 0) {
//        return res;
//    }
//
//    return vfs_access_internal(ctx, mode, vn_mode, vn_uid, vn_gid);
//}

void vfs_init(void) {
//    // Setup root node
//    strcpy(vfs_root_node.name, "[root]");
//    vfs_root_node.vnode = NULL;
//    vfs_root_node.real_vnode = NULL;
//    vfs_root_node.parent = NULL;
//    vfs_root_node.cdr = NULL;
//    vfs_root_node.child = NULL;
}

//static const char *vfs_path_element(char *dst, const char *src) {
//    const char *sep = strchr(src, '/');
//    if (!sep) {
//        strcpy(dst, src);
//        return NULL;
//    } else {
//        strncpy(dst, src, sep - src);
//        dst[sep - src] = 0;
//        while (*sep == '/') {
//            ++sep;
//        }
//        if (!*sep) {
//            return NULL;
//        }
//        return sep;
//    }
//}

//void vfs_node_free(struct vfs_node *node) {
//    _assert(node && node->vnode);
//    _assert(node->vnode->refcount == 0);
//    kfree(node);
//}
//
//struct vfs_node *vfs_node_create(const char *name, vnode_t *vn) {
//    _assert(vn);
//    struct vfs_node *node = (struct vfs_node *) kmalloc(sizeof(struct vfs_node));
//    vn->refcount = 0;
//    vn->tree_node = node;
//    node->vnode = vn;
//    strcpy(node->name, name);
//    node->ismount = 0;
//    node->real_vnode = NULL;
//    node->parent = NULL;
//    node->child = NULL;
//    node->cdr = NULL;
//    return node;
//}

/**
 * @brief The same as vfs_find, but more internal to the VFS - it operates
 *        on VFS path tree instead of vnodes (as they have no hierarchy defined)
 *
 * XXX: ".." will leave you with dangling nodes in the tree
 */
//static int vfs_find_tree(struct vfs_node *root_node, const char *path, struct vfs_node **res_node) {
//    if (!path || !*path) {
//        // The path refers to the node itself
//        *res_node = root_node;
//        return 0;
//    }
//
//    // Assuming the path is normalized
//    char path_element[256];
//    const char *child_path = vfs_path_element(path_element, path);
//
//    // TODO: this should also be handled by path canonicalizer
//    while (!strcmp(path_element, ".")) {
//        if (!child_path) {
//            // The node we're looking for is this node
//            *res_node = root_node;
//            return 0;
//        }
//        child_path = vfs_path_element(path_element, child_path);
//    }
//    if (!strcmp(path_element, "..")) {
//        if (root_node->parent) {
//            return vfs_find_tree(root_node->parent, child_path, res_node);
//        } else {
//            while (!strcmp(path_element, "..")) {
//                if (!child_path) {
//                    *res_node = root_node;
//                    return 0;
//                }
//                child_path = vfs_path_element(path_element, child_path);
//            }
//        }
//    }
//
//    vnode_t *root_vnode = root_node->vnode;
//    _assert(root_vnode);
//
//    // This shouldn't happen
//    _assert(root_vnode->type != VN_LNK);
//
//    int res;
//
//    // 1. Make sure we're either looking inside a directory
//    if (root_vnode->type == VN_DIR) {
//        // 3.1. It's a directory, try looking up path element inside
//        //      the path tree
//        for (struct vfs_node *it = root_node->child; it; it = it->cdr) {
//            if (!strcmp(it->name, path_element)) {
//                // Found matching path element
//                if (!child_path) {
//                    // We're at terminal path element - which means we've
//                    // found what we're looking for
//                    //vnode_ref(root_vnode);
//                    *res_node = it;
//                    return 0;
//                } else {
//                    //printf("Entering vfs_node %s\n", it->name);
//                    // Continue searching deeper
//                    if ((res = vfs_find_tree(it, child_path, res_node)) != 0) {
//                        // Nothing found
//                        return res;
//                    }
//
//                    // Found something
//                    return 0;
//                }
//            }
//        }
//
//        // Check if the node belongs to a mapper-based filesystem
//        _assert(root_vnode->fs && root_vnode->fs->cls);
//        if (root_vnode->fs->cls->opt & FS_NODE_MAPPER) {
//            return -ENOENT;
//        }
//
//        // 3.2. Nothing found in path tree - request the fs to find
//        //      the vnode for the path element given
//        vnode_t *child_vnode = NULL;
//        struct vfs_node *child_node = NULL;
//        _assert(root_vnode->op && root_vnode->op->find);
//
//        //printf("Calling op->find on %s\n", root_node->name);
//        if ((res = root_vnode->op->find(root_vnode, path_element, &child_vnode)) != 0) {
//            // fs didn't find anything - no such file or directory exists
//            return res;
//        }
//
//        // We've found a link and there's still some path to traverse
//        if (child_vnode->type == VN_LNK && child_path) {
//            char linkbuf[1024];
//            struct vfs_node *link_node;
//            _assert(child_vnode->op && child_vnode->op->readlink);
//            child_node = vfs_node_create(path_element, child_vnode);
//
//            if ((res = child_vnode->op->readlink(child_vnode, linkbuf)) < 0) {
//                return res;
//            }
//
//            if ((res = vfs_find_tree(root_node->parent ?
//                                     root_node->parent :
//                                     &vfs_root_node, linkbuf, &link_node)) < 0) {
//                return res;
//            }
//
//            if ((res = vfs_find_tree(link_node, child_path, res_node)) < 0) {
//                return res;
//            }
//
//            return 0;
//        }
//
//        // 3.3. Found some vnode, attach it to the VFS tree
//        child_node = vfs_node_create(path_element, child_vnode);
//
//        // Prepend it to parent's child list
//        child_node->parent = root_node;
//        child_node->cdr = root_node->child;
//        root_node->child = child_node;
//
//        if (!child_path) {
//            // We're at terminal path element - return the node
//            *res_node = child_node;
//            return 0;
//        } else {
//            //printf("Entering vfs_node %s\n", child_node->name);
//            if ((res = vfs_find_tree(child_node, child_path, res_node)) != 0) {
//                // Nothing found in the child
//                return res;
//            }
//
//            // Found what we're looking for
//            // Now have not only a child reference, but also someone uses the
//            // node down the tree, so increment the refcounter
//            return 0;
//        }
//    } else {
//        // Not a directory/mountpoint - cannot contain anything
//        return -ENOENT;
//    }
//}
//
//static int vfs_find_at(vnode_t *root_vnode, const char *path, vnode_t **res_vnode) {
//    // The input path should be without leading slash and relative to root_vnode
//    struct vfs_node *res_node = NULL;
//    int res;
//
//    if (!root_vnode) {
//        // Root node contains no vnode - which means there's no root
//        // at all
//        if (!vfs_root_node.vnode) {
//            return -ENOENT;
//        }
//
//        res = vfs_find_tree(&vfs_root_node, path, &res_node);
//    } else {
//        _assert(root_vnode->tree_node);
//
//        res = vfs_find_tree((struct vfs_node *) root_vnode->tree_node, path, &res_node);
//    }
//
//    if (res != 0) {
//        return res;
//    }
//
//    _assert(res_node);
//    *res_vnode = res_node->vnode;
//    return 0;
//}
//
//static int vfs_find(vnode_t *cwd_vnode, const char *path, vnode_t **res_vnode) {
//    if (*path != '/') {
//        return vfs_find_at(cwd_vnode, path, res_vnode);
//    } else {
//        // Use root as search base
//        while (*path == '/') {
//            ++path;
//        }
//
//        return vfs_find_at(NULL, path, res_vnode);
//    }
//}

//int vfs_mount_internal(struct vfs_node *at, void *blkdev, const char *fs_name, const char *opt) {
//    struct fs_class *fs_class;
//
//    if (!strcmp(fs_name, "auto")) {
//        return blk_mount_auto(at, blkdev, opt);
//    }
//
//    if ((fs_class = fs_class_by_name(fs_name)) == NULL) {
//        kdebug("Unknown filesystem class: %s\n", fs_name);
//        return -EINVAL;
//    }
//
//    // Create a new fs instance/mount
//    struct fs *fs;
//
//    if ((fs = fs_create(fs_class, blkdev, NULL)) == NULL) {
//        return -EINVAL;
//    }
//
//    if (!at) {
//        at = &vfs_root_node;
//    }
//
//    vnode_t *old_vnode = at->vnode;
//    vnode_t *fs_root;
//    int res;
//
//    if ((fs_class->mount != NULL) && (fs_class->mount(fs, opt) != 0)) {
//        return -1;
//    }
//
//    if (at->child) {
//        // Target directory already has child nodes loaded in memory, return "busy"
//        // TODO: destroy fs
//        return -EBUSY;
//    }
//
//    if (at->ismount) {
//        // TODO: report error and destroy fs
//        panic("Trying to mount a filesystem at a destination which already is a mount\n");
//    }
//
//    if (fs->cls->opt & FS_NODE_MAPPER) {
//        _assert(fs_class->mapper);
//        // Request the driver to map VFS tree for us
//        struct vfs_node *fs_root_node;
//
//        if ((res = fs_class->mapper(fs, &fs_root_node)) < 0) {
//            panic("Node mapper function failed\n");
//        }
//        _assert(fs_root_node);
//        _assert(fs_root_node->vnode);
//        fs_root = fs_root_node->vnode;
//
//        // Reparent vnode to the actual mountpoint
//        fs_root->tree_node = at;
//        _assert(!at->child);
//
//        // Reparent fs_root_node children to the actual mountpoint
//        struct vfs_node *child = fs_root_node->child;
//        while (child) {
//            struct vfs_node *cdr = child->cdr;
//            child->parent = at;
//            child->cdr = at->child;
//            at->child = child;
//            child = cdr;
//        }
//
//        // Root node can be freed
//        kfree(fs_root_node);
//    } else {
//        _assert(fs_class->get_root);
//        // Try to get root
//        if ((fs_root = fs_class->get_root(fs)) == NULL) {
//            // TODO: report error and destroy fs
//            panic("Failed to get root node of the filesystem\n");
//        }
//    }
//
//    // If it's a root mount, set root vnode
//    kdebug("Mounting new fs on %s\n", at->name);
//    at->vnode = fs_root;
//    at->real_vnode = old_vnode;
//    at->ismount = 1;
//    fs_root->tree_node = at;
//
//    return 0;
//}
//
//int vfs_mount(struct vfs_ioctx *ctx, const char *target, void *blkdev, const char *fs_name, const char *opt) {
//    if (ctx->uid != 0) {
//        return -EACCES;
//    }
//
//    struct vfs_node *mount_at;
//    vnode_t *vnode_mount_at;
//    int res;
//
//    if (!vfs_root_node.vnode) {
//        // Root does not yet exist, check if we're mounting root:
//        if (!strcmp(target, "/")) {
//            return vfs_mount_internal(NULL, blkdev, fs_name, opt);
//        }
//
//        // Otherwise we cannot perform mounting
//        return -ENOENT;
//    }
//
//    // Lookup the tree node we're mounting at
//    if ((res = vfs_find(ctx->cwd_vnode, target, &vnode_mount_at)) != 0) {
//        return res;
//    }
//
//    // Get tree node
//    mount_at = vnode_mount_at->tree_node;
//    _assert(mount_at);
//
//    return vfs_mount_internal(mount_at, blkdev, fs_name, opt);
//}

//int vfs_umount(struct vfs_ioctx *ctx, const char *target) {
//    if (ctx->uid != 0) {
//        return -EACCES;
//    }
//    if (!vfs_root_node.vnode) {
//        // No root, don't even bother umounting anything
//        return -ENOENT;
//    }
//
//    // Lookup target vnode's tree_node
//    _assert(target);
//    vnode_t *at_vnode;
//    struct vfs_node *at;
//    int res;
//
//    if ((res = vfs_find(ctx->cwd_vnode, target, &at_vnode)) != 0) {
//        return res;
//    }
//
//    at = at_vnode->tree_node;
//    _assert(at);
//
//    if (!at->ismount) {
//        // Not mounted
//        return -EINVAL;
//    }
//
//    if (at->child) {
//        // There're some used vnodes down the tree
//        return -EBUSY;
//    }
//
//    at->vnode = at->real_vnode;
//    at->ismount = 0;
//
//    if (at_vnode == ctx->cwd_vnode) {
//        // Umounting the cwd
//        ctx->cwd_vnode = NULL;
//    }
//    at_vnode->refcount = 0;
//    vnode_free(at_vnode);
//
//    return 0;
//}

//static void vfs_path_parent(char *dst, const char *path) {
//    // The function expects normalized paths without . and ..
//    // Possible inputs:
//    //  "/" -> "/"
//    //  "/dir/x/y" -> "/dir/x"
//
//    const char *slash = strrchr(path, '/');
//    if (!slash) {
//        dst[0] = 0;
//        return;
//    }
//
//    strncpy(dst, path, slash - path);
//    dst[slash - path] = 0;
//}

//static const char *vfs_path_basename(const char *path) {
//    const char *slash = strrchr(path, '/');
//    if (!slash) {
//        return path;
//    }
//
//    return slash + 1;
//}

//static int vfs_creat_internal(struct vfs_ioctx *ctx, vnode_t *at, const char *name, int mode, int opt, vnode_t **resvn) {
//    // Create a file without opening it
//    _assert(at && at->op && at->tree_node);
//    int res;
//
//    if (!at->op->creat) {
//        return -EROFS;
//    }
//
//    if ((res = at->op->creat(at, ctx, name, mode, opt, resvn)) != 0) {
//        return res;
//    }
//
//    struct vfs_node *parent_node = at->tree_node;
//    struct vfs_node *child_node = vfs_node_create(name, *resvn);
//
//    // Prepend it to parent's child list
//    child_node->parent = parent_node;
//    child_node->cdr = parent_node->child;
//    parent_node->child = child_node;
//
//    return 0;
//}

//int vfs_creat(struct vfs_ioctx *ctx, struct ofile *of, const char *path, int mode, int opt) {
//    vnode_t *parent_vnode = NULL;
//    vnode_t *vnode = NULL;
//    int res;
//
//    if ((res = vfs_find(ctx->cwd_vnode, path, &vnode)) == 0) {
//        vnode_ref(vnode);
//        if ((res = vfs_open_node(ctx, of, vnode, opt & ~O_CREAT)) < 0) {
//            vnode_unref(vnode);
//        }
//        return res;
//    }
//
//    if (*path == '/') {
//        // Get parent vnode
//        char parent_path[1024];
//        vfs_path_parent(parent_path, path);
//
//        if ((res = vfs_find(NULL, parent_path, &parent_vnode)) != 0) {
//            kerror("Parent does not exist: %s\n", parent_path);
//            // Parent doesn't exist, too - error
//            return res;
//        }
//    } else {
//        char parent_path[1024];
//        vfs_path_parent(parent_path, path);
//
//        if (!*parent_path) {
//            parent_path[0] = '.';
//            parent_path[1] = 0;
//        }
//
//        // Find parent
//        if ((res = vfs_find(ctx->cwd_vnode, parent_path, &parent_vnode)) != 0) {
//            kerror("Parent does not exist: %s\n", parent_path);
//            return res;
//        }
//    }
//
//    vnode_ref(parent_vnode);
//
//    if (parent_vnode->type == VN_LNK) {
//        _assert(parent_vnode->op);
//        _assert(parent_vnode->op->readlink);
//        char lnk[1024];
//        vnode_t *vn_lnk;
//        struct vfs_node *vn_node = (struct vfs_node *) parent_vnode->tree_node;
//        struct vfs_node *vn_lnk_node;
//        _assert(vn_node);
//
//        if ((res = parent_vnode->op->readlink(parent_vnode, lnk)) < 0) {
//            vnode_unref(parent_vnode);
//            return res;
//        }
//
//        if ((res = vfs_find_tree(vn_node->parent, lnk, &vn_lnk_node)) < 0) {
//            vnode_unref(parent_vnode);
//            return res;
//        }
//
//        vn_lnk = vn_lnk_node->vnode;
//        vnode_ref(vn_lnk);
//        vnode_unref(parent_vnode);
//
//        parent_vnode = vn_lnk;
//    }
//
//    if (parent_vnode->type != VN_DIR) {
//        vnode_unref(parent_vnode);
//        // Parent is not a directory
//        return -ENOTDIR;
//    }
//
//    if (vfs_vnode_access(ctx, parent_vnode, W_OK) < 0) {
//        vnode_unref(parent_vnode);
//        return -EACCES;
//    }
//
//    kdebug("Path: %s\n", path);
//    path = vfs_path_basename(path);
//
//    if (!path) {
//        return -EINVAL;
//    }
//
//    if ((res = vfs_creat_internal(ctx, parent_vnode, path, mode, opt & ~O_CREAT, &vnode)) != 0) {
//        vnode_unref(parent_vnode);
//        // Could not create entry
//        return res;
//    }
//
//    vnode_ref(vnode);
//    vnode_unref(parent_vnode);
//
//    if (!of) {
//        vnode_unref(vnode);
//        // Need opening the file, but no descriptor provided
//        return -EINVAL;
//    }
//
//    if ((res = vfs_open_node(ctx, of, vnode, opt & ~O_CREAT)) < 0) {
//        vnode_unref(vnode);
//    }
//
//    return res;
//}
//
//int vfs_open(struct vfs_ioctx *ctx, struct ofile *of, const char *path, int mode, int opt) {
//    _assert(of);
//    // Try to find the file
//    int res;
//    vnode_t *vnode = NULL;
//
//    // TODO: normalize path
//
//    if ((res = vfs_find(ctx->cwd_vnode, path, &vnode)) != 0) {
//        if (!(opt & O_CREAT)) {
//            return -ENOENT;
//        }
//
//        return vfs_creat(ctx, of, path, mode, opt);
//    }
//
//    vnode_ref(vnode);
//
//    // Resolve symlink to open the resource it's pointing to
//    if (vnode->type == VN_LNK) {
//        _assert(vnode->op);
//        _assert(vnode->op->readlink);
//        char lnk[1024];
//        vnode_t *vn_lnk;
//        struct vfs_node *vn_node = (struct vfs_node *) vnode->tree_node;
//        struct vfs_node *vn_lnk_node;
//        _assert(vn_node);
//
//        if ((res = vnode->op->readlink(vnode, lnk)) < 0) {
//            vnode_unref(vnode);
//            return res;
//        }
//
//        if ((res = vfs_find_tree(vn_node->parent, lnk, &vn_lnk_node)) < 0) {
//            vnode_unref(vnode);
//            return res;
//        }
//
//        vn_lnk = vn_lnk_node->vnode;
//        vnode_ref(vn_lnk);
//        vnode_unref(vnode);
//
//        vnode = vn_lnk;
//    }
//
//    if ((res = vfs_open_node(ctx, of, vnode, opt & ~O_CREAT)) < 0) {
//        vnode_unref(vnode);
//        return res;
//    }
//
//    // Leave refcount + 1'd, ioctx is using the node
//    return 0;
//}
//
//int vfs_open_node(struct vfs_ioctx *ctx, struct ofile *of, vnode_t *vn, int opt) {
//    // TODO: O_APPEND
//    _assert(vn && vn->op && of);
//    kdebug("vfs_open_node %p\n", of);
//    int res;
//
//    if (vfs_vnode_access(ctx, vn, vfs_open_access_mask(opt)) < 0) {
//        return -EACCES;
//    }
//
//    if (opt & O_DIRECTORY) {
//        _assert((opt & O_ACCMODE) == O_RDONLY);
//        // How does one truncate a directory?
//        _assert(!(opt & O_TRUNC));
//        _assert(!(opt & O_CREAT));
//
//        if (vn->type != VN_DIR) {
//            return -ENOTDIR;
//        }
//
//        _assert(vn->fs && vn->fs->cls);
//        if (vn->fs->cls->opt & FS_NODE_MAPPER) {
//            // XXX: This is totally read-only
//            // Mapper means all the needed nodes are already in vnode tree
//            // just use (vfs_node *) as offset
//            _assert(sizeof(of->pos) >= sizeof(uintptr_t));
//            _assert(vn->tree_node);
//
//            of->pos = (uintptr_t) ((struct vfs_node *) vn->tree_node)->child;
//            of->flags = opt;
//            of->vnode = vn;
//
//            return 0;
//        } else {
//            // opendir
//            if (vn->op->opendir) {
//                if ((res = vn->op->opendir(vn, opt)) != 0) {
//                    return res;
//                }
//            } else {
//                return -EINVAL;
//            }
//
//            of->flags = opt;
//            of->vnode = vn;
//            of->pos = 0;
//        }
//
//        return res;
//    }
//
//    // Check flag sanity
//    // Can't have O_CREAT here
//    if (opt & O_CREAT) {
//        return -EINVAL;
//    }
//    // Can't be both (RD|WR) and EX
//    if (opt & O_EXEC) {
//        if (opt & O_ACCMODE) {
//            return -EACCES;
//        }
//    }
//
//    if (vn->type == VN_DIR) {
//        return -EISDIR;
//    }
//
//    of->vnode = vn;
//    of->flags = opt;
//    of->pos = 0;
//
//    if (opt & O_APPEND) {
//        // TODO: rewrite open() to accept struct ofile *
//        // instead of vnode so that open() function of the
//        // vnode can properly set of->pos
//        //fprintf(stderr, "O_APPEND not yet implemented\n");
//        kerror("O_APPEND not yet implemented\n");
//        return -EINVAL;
//    }
//
//    // Check if file has to be truncated before opening it
//    if (opt & O_TRUNC) {
//        if (!vn->op->truncate) {
//            return -EINVAL;
//        }
//
//        if ((res = vn->op->truncate(of, 0)) != 0) {
//            return res;
//        }
//    }
//
//    if (vn->op->open) {
//        if ((res = vn->op->open(vn, opt)) != 0) {
//            return res;
//        }
//    }
//
//    return 0;
//}
//
//void vfs_close(struct vfs_ioctx *ctx, struct ofile *of) {
//    _assert(of);
//    vnode_t *vn = of->vnode;
//    switch (vn->type) {
//    case VN_REG:
//    case VN_DIR:
//        _assert(vn);
//
//        if (vn->op->close) {
//            vn->op->close(of);
//        }
//        break;
//
//    // TODO: maybe call something special for that
//    case VN_CHR:
//    case VN_BLK:
//        break;
//
//    default:
//        panic("Unhandled vnode type\n");
//    }
//
//    vnode_unref(of->vnode);
//}
//
//int vfs_statat(struct vfs_ioctx *ctx, vnode_t *at, const char *path, struct stat *st) {
//    _assert(at && path && st);
//    int res;
//    vnode_t *vnode;
//
//    if ((res = vfs_find(at, path, &vnode)) != 0) {
//        return res;
//    }
//
//    vnode_ref(vnode);
//
//    if (!vnode->op || !vnode->op->stat) {
//        res = -EINVAL;
//    } else {
//        res = vnode->op->stat(vnode, st);
//    }
//
//    vnode_unref(vnode);
//    return res;
//}
//
//int vfs_stat(struct vfs_ioctx *ctx, const char *path, struct stat *st) {
//    _assert(path && st);
//    vnode_t *vnode;
//    int res;
//
//    if ((res = vfs_find(ctx->cwd_vnode, path, &vnode)) != 0) {
//        return res;
//    }
//
//    vnode_ref(vnode);
//
//    if (!vnode->op || !vnode->op->stat) {
//        res = -EINVAL;
//    } else {
//        res = vnode->op->stat(vnode, st);
//    }
//
//    vnode_unref(vnode);
//    return res;
//}
//
//ssize_t vfs_read(struct vfs_ioctx *ctx, struct ofile *fd, void *buf, size_t count) {
//    _assert(fd);
//    vnode_t *vn = fd->vnode;
//    _assert(vn);
//
//    ssize_t nr;
//
//    switch (vn->type) {
//    case VN_REG:
//        _assert(vn->op);
//        // XXX: should these be checked on every read?
//        if (vfs_vnode_access(ctx, vn, R_OK) < 0) {
//            return -EACCES;
//        }
//
//        if (fd->flags & O_DIRECTORY) {
//            return -EISDIR;
//        }
//        if ((fd->flags & O_ACCMODE) == O_WRONLY) {
//            return -EINVAL;
//        }
//        if (vn->op->read == NULL) {
//            return -EINVAL;
//        }
//
//        nr = vn->op->read(fd, buf, count);
//
//        if (nr > 0) {
//            fd->pos += nr;
//        }
//
//        return nr;
//
//    // We don't need filesystem at all to read from devices
//    case VN_BLK:
//        _assert(vn->dev);
//
//        if ((nr = blk_read((struct blkdev *) vn->dev, buf, fd->pos, count)) > 0) {
//            fd->pos += nr;
//        }
//
//        return nr;
//    case VN_CHR:
//        _assert(vn->dev);
//        return chr_read((struct chrdev *) vn->dev, buf, fd->pos, count);
//
//    default:
//        panic("Not supported\n");
//    }
//}
//
//ssize_t vfs_write(struct vfs_ioctx *ctx, struct ofile *fd, const void *buf, size_t count) {
//    _assert(fd);
//    vnode_t *vn = fd->vnode;
//    _assert(vn);
//
//    switch (vn->type) {
//    case VN_REG:
//        _assert(vn->op);
//
//        // XXX: should these be checked on every write?
//        if (vfs_vnode_access(ctx, vn, W_OK) < 0) {
//            return -EACCES;
//        }
//
//        if (fd->flags & O_DIRECTORY) {
//            return -EISDIR;
//        }
//        if ((fd->flags & O_ACCMODE) == O_RDONLY) {
//            return -EINVAL;
//        }
//        if (vn->op->write == NULL) {
//            return -EINVAL;
//        }
//
//        return vn->op->write(fd, buf, count);
//
//    // We don't need filesystem at all to write to devices
//    case VN_BLK:
//        _assert(vn->dev);
//        return blk_write((struct blkdev *) vn->dev, buf, fd->pos, count);
//    case VN_CHR:
//        _assert(vn->dev);
//        return chr_write((struct chrdev *) vn->dev, buf, fd->pos, count);
//
//    default:
//        panic("Not supported\n");
//    }
//}

//int vfs_truncate(struct vfs_ioctx *ctx, struct ofile *of, size_t length) {
//    _assert(of);
//    if ((of->flags & O_ACCMODE) == O_RDONLY) {
//        return -EINVAL;
//    }
//    if ((of->flags & O_DIRECTORY)) {
//        return -EINVAL;
//    }
//    vnode_t *vn = of->vnode;
//    _assert(vn && vn->op);
//    // XXX: should these be checked on every write?
//    if (vfs_vnode_access(ctx, vn, W_OK) < 0) {
//        return -EACCES;
//    }
//
//    if (!vn->op->truncate) {
//        return -EINVAL;
//    }
//
//    return vn->op->truncate(of, length);
//}
//
//int vfs_unlink(struct vfs_ioctx *ctx, const char *path, int is_rmdir) {
//    // XXX: validate this with removing mounted roots
//    _assert(path);
//    // Find the vnode to unlink
//    int res;
//    vnode_t *parent_vnode, *vnode;
//
//    if ((res = vfs_find(ctx->cwd_vnode, path, &vnode)) < 0) {
//        return res;
//    }
//
//    _assert(vnode && vnode->op);
//    vnode_ref(vnode);
//
//    if (vnode->type != VN_DIR && is_rmdir) {
//        return -ENOTDIR;
//    }
//    if (vnode->type == VN_DIR && !is_rmdir) {
//        return -EISDIR;
//    }
//
//    // Get node parent
//    struct vfs_node *node = vnode->tree_node;
//    _assert(node);
//
//    if (!node->parent) {
//        // Trying to unlink root node?
//        vnode_unref(vnode);
//        return -EACCES;
//    }
//
//    if (vnode->refcount != 0) {
//        // Trying to unlink the file someone is using.
//        // Good solution would be to (TODO) defer the
//        // actual unlinking and perform it once no one
//        // is using it or notify writers/reader that the
//        // file is slated for removal. I think just adding
//        // a flag to vnode like "deleted" should suffice.
//        // For now, just check if vfs_ctx is trying to shoot
//        // its' leg off by unlinking the CWD
//        if (vnode == ctx->cwd_vnode) {
//            return -EINVAL;
//        }
//    }
//
//    parent_vnode = node->parent->vnode;
//    vnode_ref(parent_vnode);
//
//    // Can only remove a child in a directory
//    if (parent_vnode->type != VN_DIR) {
//        vnode_unref(vnode);
//        vnode_unref(parent_vnode);
//        return res;
//    }
//
//    if ((res = vfs_vnode_access(ctx, parent_vnode, W_OK)) < 0) {
//        vnode_unref(vnode);
//        vnode_unref(parent_vnode);
//        return res;
//    }
//
//    if (parent_vnode->op->unlink) {
//        // TODO: handle
//        //       unlink("path/to/node/./.")
//        path = vfs_path_basename(path);
//        _assert(path);
//
//        if ((res = parent_vnode->op->unlink(parent_vnode, vnode, path)) < 0) {
//            vnode_unref(vnode);
//            vnode_unref(parent_vnode);
//            return res;
//        }
//
//        vnode_unref(vnode);
//        vnode_unref(parent_vnode);
//        return 0;
//    } else {
//        //fprintf(stderr, "File system does not implement unlink()\n");
//        kerror("Filesystem does not implement unlink()\n");
//        // File node does not support unlinking
//        vnode_unref(vnode);
//        vnode_unref(parent_vnode);
//        return -EINVAL;
//    }
//}
//
//int vfs_chmod(struct vfs_ioctx *ctx, const char *path, mode_t mode) {
//    _assert(path);
//    vnode_t *vnode;
//    int res;
//
//    if ((res = vfs_find(ctx->cwd_vnode, path, &vnode)) < 0) {
//        return res;
//    }
//
//    vnode_ref(vnode);
//    _assert(vnode && vnode->op);
//
//    if (vnode->op->access) {
//        mode_t vn_mode;
//        uid_t vn_uid;
//        gid_t vn_gid;
//
//        if ((res = vnode->op->access(vnode, &vn_uid, &vn_gid, &vn_mode)) < 0) {
//            return res;
//        }
//
//        // To chmod, the uid of the user has to match
//        // the node's one
//        if ((vn_uid != ctx->uid) && (ctx->uid != 0)) {
//            return -EACCES;
//        }
//    }
//
//    if (!vnode->op->chmod) {
//        return -EINVAL;
//    }
//
//    res = vnode->op->chmod(vnode, mode);
//
//    vnode_unref(vnode);
//    return res;
//}
//
//int vfs_chown(struct vfs_ioctx *ctx, const char *path, uid_t uid, gid_t gid) {
//    _assert(path);
//    vnode_t *vnode;
//    int res;
//
//    if (ctx->uid != 0) {
//        // For now, only root can change ownership of the nodes
//        return -EACCES;
//    }
//
//    if ((res = vfs_find(ctx->cwd_vnode, path, &vnode)) < 0) {
//        return res;
//    }
//
//    vnode_ref(vnode);
//    _assert(vnode && vnode->op);
//
//    if (!vnode->op->chown) {
//        return -EINVAL;
//    }
//
//    res = vnode->op->chown(vnode, uid, gid);
//
//    vnode_unref(vnode);
//    return res;
//}
//
//// TODO: change signature so it can return errno
//struct dirent *vfs_readdir(struct vfs_ioctx *ctx, struct ofile *fd) {
//    _assert(fd);
//    if (!(fd->flags & O_DIRECTORY)) {
//        return NULL;
//    }
//    vnode_t *vn = fd->vnode;
//    _assert(vn && vn->op);
//
//    if (vfs_vnode_access(ctx, vn, R_OK) < 0) {
//        return NULL;
//    }
//
//    _assert(vn->fs && vn->fs->cls);
//    if (vn->fs->cls->opt & FS_NODE_MAPPER) {
//        _assert(sizeof(fd->pos) >= sizeof(uintptr_t));
//        struct vfs_node *item_node = (struct vfs_node *) fd->pos;
//        struct dirent *ent_buf = (struct dirent *) fd->dirent_buf;
//
//        if (item_node) {
//            fd->pos = (uintptr_t) (item_node->cdr);
//        } else {
//            return NULL;
//        }
//
//        _assert(item_node->vnode);
//
//        // Describe the node into dirent buffer
//        // TODO: size checks
//        strcpy(ent_buf->d_name, item_node->name);
//        ent_buf->d_ino = /* TODO use something as inode number for node mapper */ -1;
//        ent_buf->d_off = 0;
//        ent_buf->d_reclen = strlen(item_node->name) + sizeof(struct dirent);
//        // TODO: blk/chr
//        switch (item_node->vnode->type) {
//        case VN_REG:
//            ent_buf->d_type = DT_REG;
//            break;
//        case VN_DIR:
//            ent_buf->d_type = DT_DIR;
//            break;
//        case VN_BLK:
//            ent_buf->d_type = DT_BLK;
//            break;
//        case VN_CHR:
//            ent_buf->d_type = DT_CHR;
//            break;
//        default:
//            kwarn("Unsupported vnode type: %d\n", item_node->vnode->type);
//            ent_buf->d_type = DT_UNKNOWN;
//            break;
//        }
//
//        return ent_buf;
//    } else {
//        if (!vn->op->readdir) {
//            return NULL;
//        }
//
//        if (vn->op->readdir(fd) == 0) {
//            return (struct dirent *) fd->dirent_buf;
//        }
//
//        return NULL;
//    }
//}
//
//int vfs_mkdir(struct vfs_ioctx *ctx, const char *path, mode_t mode) {
//    vnode_t *parent_vnode = NULL;
//    vnode_t *vnode = NULL;
//    int res;
//
//    // Check if a directory with such name already exists
//    if ((res = vfs_find(ctx->cwd_vnode, path, &vnode)) == 0) {
//        vnode_ref(vnode);
//        vnode_unref(vnode);
//        return -EEXIST;
//    }
//
//    // Just copypasted this from creat()
//    if (*path == '/') {
//        // Get parent vnode
//        char parent_path[1024];
//        vfs_path_parent(parent_path, path);
//
//        if ((res = vfs_find(NULL, parent_path, &parent_vnode)) != 0) {
//            kerror("Parent does not exist: %s\n", parent_path);
//            // Parent doesn't exist, too - error
//            return res;
//        }
//    } else {
//        char parent_path[1024];
//        vfs_path_parent(parent_path, path);
//
//        if (!*parent_path) {
//            parent_path[0] = '.';
//            parent_path[1] = 0;
//        }
//
//        // Find parent
//        if ((res = vfs_find(ctx->cwd_vnode, parent_path, &parent_vnode)) != 0) {
//            kerror("Parent does not exist: %s\n", parent_path);
//            return res;
//        }
//    }
//
//    vnode_ref(parent_vnode);
//
//    if (parent_vnode->type == VN_LNK) {
//        _assert(parent_vnode->op);
//        _assert(parent_vnode->op->readlink);
//        char lnk[1024];
//        vnode_t *vn_lnk;
//        struct vfs_node *vn_node = (struct vfs_node *) parent_vnode->tree_node;
//        struct vfs_node *vn_lnk_node;
//        _assert(vn_node);
//
//        if ((res = parent_vnode->op->readlink(parent_vnode, lnk)) < 0) {
//            vnode_unref(parent_vnode);
//            return res;
//        }
//
//        if ((res = vfs_find_tree(vn_node->parent, lnk, &vn_lnk_node)) < 0) {
//            vnode_unref(parent_vnode);
//            return res;
//        }
//
//        vn_lnk = vn_lnk_node->vnode;
//        vnode_ref(vn_lnk);
//        vnode_unref(parent_vnode);
//
//        parent_vnode = vn_lnk;
//    }
//
//    if (parent_vnode->type != VN_DIR) {
//        // Parent is not a directory
//        vnode_unref(parent_vnode);
//        return -ENOTDIR;
//    }
//
//    // Need write permission
//    if ((res = vfs_vnode_access(ctx, parent_vnode, W_OK)) < 0) {
//        vnode_unref(parent_vnode);
//        return res;
//    }
//
//    kdebug("Path: %s\n", path);
//    path = vfs_path_basename(path);
//
//    if (!path) {
//        vnode_unref(parent_vnode);
//        return -EINVAL;
//    }
//
//    if (!parent_vnode->op || !parent_vnode->op->mkdir) {
//        vnode_unref(parent_vnode);
//        return -EINVAL;
//    }
//
//    if ((res = parent_vnode->op->mkdir(parent_vnode, path, mode)) < 0) {
//        vnode_unref(parent_vnode);
//        return res;
//    }
//
//    vnode_unref(parent_vnode);
//    return res;
//}
//
//int vfs_access(struct vfs_ioctx *ctx, const char *path, int mode) {
//    _assert(path);
//    vnode_t *vnode;
//    int res;
//
//    mode_t vn_mode;
//    uid_t vn_uid;
//    gid_t vn_gid;
//
//    if ((res = vfs_find(ctx->cwd_vnode, path, &vnode)) != 0) {
//        return res;
//    }
//
//    vnode_ref(vnode);
//    if (mode == F_OK) {
//        vnode_unref(vnode);
//        return 0;
//    }
//
//    _assert(vnode->op);
//
//    if (!vnode->op->access) {
//        vnode_unref(vnode);
//        // Filesystem has no permissions?
//        return -EINVAL;
//    }
//
//    if ((res = vnode->op->access(vnode, &vn_uid, &vn_gid, &vn_mode)) < 0) {
//        vnode_unref(vnode);
//        return res;
//    }
//
//    vnode_unref(vnode);
//
//    return vfs_access_internal(ctx, mode, vn_mode, vn_uid, vn_gid);
//}
//
//int vfs_statvfs(struct vfs_ioctx *ctx, const char *path, struct statvfs *st) {
//    _assert(ctx && path && st);
//    vnode_t *vnode;
//    int res;
//    fs_t *fs;
//
//    if ((res = vfs_find(ctx->cwd_vnode, path, &vnode)) < 0) {
//        return res;
//    }
//
//    vnode_ref(vnode);
//
//    if (!(fs = vnode->fs)) {
//        vnode_unref(vnode);
//        return -EINVAL;
//    }
//
//    vnode_unref(vnode);
//
//    if (!fs->cls || !fs->cls->statvfs) {
//        return -EINVAL;
//    }
//
//    return fs->cls->statvfs(fs, st);
//}
//
//int vfs_readlinkat(struct vfs_ioctx *ctx, vnode_t *at, const char *path, char *buf) {
//    int res;
//    vnode_t *vnode;
//
//    if ((res = vfs_find(at, path, &vnode)) < 0) {
//        return res;
//    }
//
//    vnode_ref(vnode);
//
//    if (!vnode->op || !vnode->op->readlink) {
//        vnode_unref(vnode);
//        return -EINVAL;
//    }
//    if (vnode->type != VN_LNK) {
//        vnode_unref(vnode);
//        return -EINVAL;
//    }
//
//    res = vnode->op->readlink(vnode, buf);
//    vnode_unref(vnode);
//    return res;
//}
//
//int vfs_readlink(struct vfs_ioctx *ctx, const char *path, char *buf) {
//    int res;
//    vnode_t *vnode;
//
//    if ((res = vfs_find(ctx->cwd_vnode, path, &vnode)) < 0) {
//        return res;
//    }
//
//    vnode_ref(vnode);
//
//    if (!vnode->op || !vnode->op->readlink) {
//        vnode_unref(vnode);
//        return -EINVAL;
//    }
//    if (vnode->type != VN_LNK) {
//        vnode_unref(vnode);
//        return -EINVAL;
//    }
//
//    res = vnode->op->readlink(vnode, buf);
//    vnode_unref(vnode);
//    return res;
//}
//
//int vfs_symlink(struct vfs_ioctx *ctx, const char *target, const char *linkpath) {
//    vnode_t *parent_vnode = NULL;
//    vnode_t *vnode = NULL;
//    int res;
//
//    // Check if a directory with such name already exists
//    if ((res = vfs_find(ctx->cwd_vnode, linkpath, &vnode)) == 0) {
//        vnode_ref(vnode);
//        vnode_unref(vnode);
//        return -EEXIST;
//    }
//
//    // Just copypasted this from creat()
//    if (*linkpath == '/') {
//        // Get parent vnode
//        char parent_path[1024];
//        vfs_path_parent(parent_path, linkpath);
//
//        if ((res = vfs_find(NULL, parent_path, &parent_vnode)) != 0) {
//            kerror("Parent does not exist: %s\n", parent_path);
//            // Parent doesn't exist, too - error
//            return res;
//        }
//    } else {
//        char parent_path[1024];
//        vfs_path_parent(parent_path, linkpath);
//
//        if (!*parent_path) {
//            parent_path[0] = '.';
//            parent_path[1] = 0;
//        }
//
//        // Find parent
//        if ((res = vfs_find(ctx->cwd_vnode, parent_path, &parent_vnode)) != 0) {
//            kerror("Parent does not exist: %s\n", parent_path);
//            return res;
//        }
//    }
//
//    vnode_ref(parent_vnode);
//
//    if (parent_vnode->type == VN_LNK) {
//        _assert(parent_vnode->op);
//        _assert(parent_vnode->op->readlink);
//        char lnk[1024];
//        vnode_t *vn_lnk;
//        struct vfs_node *vn_node = (struct vfs_node *) parent_vnode->tree_node;
//        struct vfs_node *vn_lnk_node;
//        _assert(vn_node);
//
//        if ((res = parent_vnode->op->readlink(parent_vnode, lnk)) < 0) {
//            vnode_unref(parent_vnode);
//            return res;
//        }
//
//        if ((res = vfs_find_tree(vn_node->parent, lnk, &vn_lnk_node)) < 0) {
//            vnode_unref(parent_vnode);
//            return res;
//        }
//
//        vn_lnk = vn_lnk_node->vnode;
//        vnode_ref(vn_lnk);
//        vnode_unref(parent_vnode);
//
//        parent_vnode = vn_lnk;
//    }
//
//    if (parent_vnode->type != VN_DIR) {
//        // Parent is not a directory
//        vnode_unref(parent_vnode);
//        return -ENOTDIR;
//    }
//
//    // Need write permission
//    if ((res = vfs_vnode_access(ctx, parent_vnode, W_OK)) < 0) {
//        vnode_unref(parent_vnode);
//        return res;
//    }
//
//    kdebug("Path: %s\n", linkpath);
//    linkpath = vfs_path_basename(linkpath);
//
//    if (!linkpath) {
//        vnode_unref(parent_vnode);
//        return -EINVAL;
//    }
//
//    if (!parent_vnode->op || !parent_vnode->op->symlink) {
//        vnode_unref(parent_vnode);
//        return -EINVAL;
//    }
//
//    // if ((res = parent_vnode->op->mkdir(parent_vnode, linkpath, mode)) < 0) {
//    //     vnode_unref(parent_vnode);
//    //     return res;
//    // }
//    if ((res = parent_vnode->op->symlink(parent_vnode, ctx, linkpath, target)) < 0) {
//        vnode_unref(parent_vnode);
//        return res;
//    }
//
//    vnode_unref(parent_vnode);
//    return res;
//}
