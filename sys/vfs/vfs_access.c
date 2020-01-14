#include "sys/fs/vfs.h"
#include "sys/assert.h"
#include "sys/fcntl.h"
#include "sys/errno.h"
#include "sys/debug.h"

int vfs_access_check(struct vfs_ioctx *ctx, int desm, mode_t mode, uid_t uid, gid_t gid) {
    if (ctx->uid == 0) {
        if (desm & X_OK) {
            // Check if anyone at all can execute this
            if (!(mode & (S_IXOTH | S_IXGRP | S_IXUSR))) {
                return -EACCES;
            }
        }

        return 0;
    }

    if (uid == ctx->uid) {
        if ((desm & R_OK) && !(mode & S_IRUSR)) {
            return -EACCES;
        }
        if ((desm & W_OK) && !(mode & S_IWUSR)) {
            return -EACCES;
        }
        if ((desm & X_OK) && !(mode & S_IXUSR)) {
            return -EACCES;
        }
    } else if (gid == ctx->gid) {
        if ((desm & R_OK) && !(mode & S_IRGRP)) {
            return -EACCES;
        }
        if ((desm & W_OK) && !(mode & S_IWGRP)) {
            return -EACCES;
        }
        if ((desm & X_OK) && !(mode & S_IXGRP)) {
            return -EACCES;
        }
    } else {
        if ((desm & R_OK) && !(mode & S_IROTH)) {
            return -EACCES;
        }
        if ((desm & W_OK) && !(mode & S_IWOTH)) {
            return -EACCES;
        }
        if ((desm & X_OK) && !(mode & S_IXOTH)) {
            return -EACCES;
        }
    }

    return 0;
}

int vfs_access_node(struct vfs_ioctx *ctx, struct vnode *vn, int mode) {
    mode_t vn_mode;
    uid_t vn_uid;
    gid_t vn_gid;
    int res;

    _assert(ctx);
    _assert(vn);

    if (!vn->op || !vn->op->access) {
        vn_mode = vn->mode;
        vn_uid = vn->uid;
        vn_gid = vn->gid;
    } else {
        // Filesystem has non-trivial permission storage
        if ((res = vn->op->access(vn, &vn_uid, &vn_gid, &vn_mode)) != 0) {
            return res;
        }
    }

    kdebug("%u:%u requset %d access to %s\n", ctx->uid, ctx->gid, mode, vn->name);

    return vfs_access_check(ctx, mode, vn_mode, vn_uid, vn_gid);
}
