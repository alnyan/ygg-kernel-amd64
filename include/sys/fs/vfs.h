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

struct vfs_ioctx {
    // Process' current working directory
    struct vnode *cwd_vnode;
    uid_t uid;
    gid_t gid;
};

void vfs_init(void);
