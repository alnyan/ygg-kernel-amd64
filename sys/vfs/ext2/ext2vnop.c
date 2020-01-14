// ext2fs vnode operations
#include "sys/fs/ext2.h"
#include "sys/fs/node.h"
#include "sys/fs/ofile.h"
#include "sys/fcntl.h"
#include "sys/fs/vfs.h"
#include "sys/string.h"
#include "sys/debug.h"
#include "sys/assert.h"
#include "sys/panic.h"
#include "sys/errno.h"
#include "sys/heap.h"

// #include <string.h>
// #include <stddef.h>
// #include <stdlib.h>
// #include <assert.h>
// #include <stdio.h>
// #include <errno.h>

// Forward declaration of ext2 vnode functions
static int ext2_vnode_find(struct vnode *vn, const char *name, struct vnode **resvn);
static int ext2_vnode_creat(struct vnode *at, const char *name, uid_t uid, gid_t gid, mode_t mode);
//static int ext2_vnode_mkdir(vnode_t *at, const char *name, mode_t mode);
static int ext2_vnode_open(struct ofile *fd, int opt);
static int ext2_vnode_opendir(struct ofile *fd);
static ssize_t ext2_vnode_read(struct ofile *fd, void *buf, size_t count);
static ssize_t ext2_vnode_write(struct ofile *fd, const void *buf, size_t count);
static int ext2_vnode_truncate(struct vnode *vn, size_t length);
static ssize_t ext2_vnode_readdir(struct ofile *fd, struct dirent *ent);
//static void ext2_vnode_destroy(vnode_t *vn);
static int ext2_vnode_stat(struct vnode *vn, struct stat *st);
//static int ext2_vnode_chmod(vnode_t *vn, mode_t mode);
//static int ext2_vnode_chown(vnode_t *vn, uid_t uid, gid_t gid);
//static int ext2_vnode_unlink(vnode_t *at, vnode_t *vn, const char *name);
//static int ext2_vnode_access(vnode_t *vn, uid_t *uid, gid_t *gid, mode_t *mode);
static int ext2_vnode_readlink(struct vnode *vn, char *dst, size_t lim);
//static int ext2_vnode_symlink(vnode_t *at, struct vfs_ioctx *ctx, const char *name, const char *dst);
static off_t ext2_vnode_lseek(struct ofile *fd, off_t offset, int whence);

struct vnode_operations ext2_vnode_ops = {
    .find = ext2_vnode_find,
    .creat = ext2_vnode_creat,
    .lseek = ext2_vnode_lseek,
//    .mkdir = ext2_vnode_mkdir,
//    .destroy = ext2_vnode_destroy,
//
    .readlink = ext2_vnode_readlink,
//    .symlink = ext2_vnode_symlink,
//
//    .chmod = ext2_vnode_chmod,
//    .chown = ext2_vnode_chown,
    .stat = ext2_vnode_stat,
//    .unlink = ext2_vnode_unlink,
//    .access = ext2_vnode_access,
//
    .opendir = ext2_vnode_opendir,
    .readdir = ext2_vnode_readdir,

    .open = ext2_vnode_open,
    .read = ext2_vnode_read,
    .write = ext2_vnode_write,
    .truncate = ext2_vnode_truncate,
};

//// vnode function implementation

static int ext2_vnode_find(struct vnode *vn, const char *name, struct vnode **res) {
    struct fs *ext2 = vn->fs;
    struct ext2_extsb *sb = vn->fs->fs_private;
    struct ext2_inode *inode = vn->fs_data;

    char buffer[sb->block_size];
    struct ext2_dirent *dirent = NULL;

    size_t block_count = (inode->size_lower + (sb->block_size - 1)) / sb->block_size;
    // char ent_name[256];
    size_t index = 0;

    while (index < block_count) {
        // Read directory contents block
        if (ext2_read_inode_block(ext2, inode, index, buffer) < 0) {
            return -EIO;
        }

        size_t offset = 0;
        while (1) {
            dirent = (struct ext2_dirent *) &buffer[offset];
            if (!dirent->len) {
                break;
            }
            if (dirent->ino) {
                if (strlen(name) == dirent->name_len && !strncmp(dirent->name, name, dirent->name_len)) {
                    // Found the entry
                    inode = kmalloc(sb->inode_struct_size);
                    if (ext2_read_inode(ext2, inode, dirent->ino) != 0) {
                        return -EIO;
                    }

                    struct vnode *node = vnode_create(ext2_inode_type(inode), name);
                    node->fs = ext2;
                    node->fs_data = inode;
                    node->ino = dirent->ino;
                    node->op = &ext2_vnode_ops;

                    node->mode = inode->type_perm & 0x1FF;
                    node->uid = inode->uid;
                    node->gid = inode->gid;

                    *res = node;

                    return 0;
                }
            }
            offset += dirent->len;
            if (offset >= sb->block_size) {
                break;
            }
        }

        ++index;
    }

    return -ENOENT;
}

static int ext2_vnode_opendir(struct ofile *fd) {
    _assert(fd);
    _assert(fd->vnode);
    _assert(fd->vnode->type == VN_DIR);

    fd->pos = 0;

    return 0;
}

static int ext2_vnode_open(struct ofile *fd, int opt) {
    _assert(fd);
    _assert(fd->vnode);
    _assert(fd->vnode->type == VN_REG);

    // TODO: O_APPEND should place fd->pos at the end of the file
    fd->pos = 0;

    return 0;
}
//
//static int ext2_vnode_mkdir(vnode_t *at, const char *name, mode_t mode) {
//    fs_t *ext2 = at->fs;
//    _assert(at->type == VN_DIR);
//    struct ext2_extsb *sb = (struct ext2_extsb *) ext2->fs_private;
//    char block_buffer[sb->block_size];
//
//    uint32_t new_ino, new_block_no;
//    int res;
//
//    // Allocate a new inode for the directory
//    if ((res = ext2_alloc_inode(ext2, &new_ino)) != 0) {
//        kerror("ext2: Failed to allocate an inode\n");
//        return res;
//    }
//
//    // Allocate a block for "." and ".." entries
//    if ((res = ext2_alloc_block(ext2, &new_block_no)) < 0) {
//        kerror("ext2: Failed to allocate a block\n");
//        return res;
//    }
//
//    struct ext2_inode *ent_inode = (struct ext2_inode *) kmalloc(sb->inode_struct_size);
//
//    // Now create an entry in parents dirent list
//    if ((res = ext2_dir_add_inode(ext2, at, name, new_ino)) < 0) {
//        return res;
//    }
//
//    // Fill the inode
//    ent_inode->flags = 0;
//    ent_inode->dir_acl = 0;
//    ent_inode->frag_block_addr = 0;
//    ent_inode->gen_number = 0;
//    ent_inode->hard_link_count = 1;
//    ent_inode->acl = 0;
//    ent_inode->os_value_1 = 0;
//    memset(ent_inode->os_value_2, 0, sizeof(ent_inode->os_value_2));
//    // TODO: time support in kernel
//    ent_inode->atime = 0;
//    ent_inode->mtime = 0;
//    ent_inode->ctime = 0;
//    ent_inode->dtime = 0;
//
//    memset(ent_inode->direct_blocks, 0, sizeof(ent_inode->direct_blocks));
//    ent_inode->direct_blocks[0] = new_block_no;
//    ent_inode->l1_indirect_block = 0;
//    ent_inode->l2_indirect_block = 0;
//    ent_inode->l3_indirect_block = 0;
//
//    ent_inode->type_perm = (mode & 0x1FF) | EXT2_TYPE_DIR;
//    // TODO: obtain these from process context in kernel
//    ent_inode->uid = 0;
//    ent_inode->gid = 0;
//    ent_inode->disk_sector_count = 0;
//    ent_inode->size_lower = sb->block_size;
//
//    memset(block_buffer, 0, sb->block_size);
//    // "."
//    struct ext2_dirent *dirent = (struct ext2_dirent *) block_buffer;
//    dirent->ino = new_ino;
//    dirent->name_len = 1;
//    dirent->len = (sizeof(struct ext2_dirent) + 4) & ~3;
//    dirent->name[0] = '.';
//    dirent->type_ind = 0;
//
//    // ".."
//    dirent = (struct ext2_dirent *) &block_buffer[dirent->len];
//    dirent->ino = at->fs_number;
//    dirent->name_len = 2;
//    dirent->len = sb->block_size - ((sizeof(struct ext2_dirent) + 4) & ~3);
//    dirent->name[0] = '.';
//    dirent->name[1] = '.';
//    dirent->type_ind = 0;
//
//    // Write directory's first block
//    if ((res = ext2_write_block(ext2, new_block_no, block_buffer)) < 0) {
//        return res;
//    }
//
//    // Write directory inode
//    if ((res = ext2_write_inode(ext2, ent_inode, new_ino)) < 0) {
//        return res;
//    }
//
//    return 0;
//}
//
static int ext2_vnode_creat(struct vnode *at, const char *name, uid_t uid, gid_t gid, mode_t mode) {
    struct fs *ext2 = at->fs;
    _assert(at->type == VN_DIR);
    _assert(/* Don't support making directories like this */ !(mode & O_DIRECTORY));
    struct ext2_extsb *sb = (struct ext2_extsb *) ext2->fs_private;

    uint32_t new_ino;
    int res;

    // Allocate new inode number
    if ((res = ext2_alloc_inode(ext2, &new_ino)) != 0) {
        kerror("Failed to allocate inode\n");
        return res;
    }

    kdebug("Allocated inode %d\n", new_ino);

    // Create an inode struct in memory
    struct ext2_inode *ent_inode = (struct ext2_inode *) kmalloc(sb->inode_struct_size);

    // Now create an entry in parents dirent list
    if ((res = ext2_dir_add_inode(ext2, at, name, new_ino)) < 0) {
        return res;
    }

    // Fill the inode
    ent_inode->flags = 0;
    ent_inode->dir_acl = 0;
    ent_inode->frag_block_addr = 0;
    ent_inode->gen_number = 0;
    ent_inode->hard_link_count = 1;
    ent_inode->acl = 0;
    ent_inode->os_value_1 = 0;
    memset(ent_inode->os_value_2, 0, sizeof(ent_inode->os_value_2));
    // TODO: time support in kernel
    ent_inode->atime = 0;
    ent_inode->mtime = 0;
    ent_inode->ctime = 0;
    ent_inode->dtime = 0;

    memset(ent_inode->direct_blocks, 0, sizeof(ent_inode->direct_blocks));
    ent_inode->l1_indirect_block = 0;
    ent_inode->l2_indirect_block = 0;
    ent_inode->l3_indirect_block = 0;

    ent_inode->uid = uid;
    ent_inode->gid = gid;
    // TODO: only regular files can be created this way now
    ent_inode->type_perm = (mode & 0x1FF) | (EXT2_TYPE_REG);
    ent_inode->disk_sector_count = 0;
    ent_inode->size_lower = 0;

    // Write the inode
    if ((res = ext2_write_inode(ext2, ent_inode, new_ino)) < 0) {
        return res;
    }

    kfree(ent_inode);

    return 0;
}
//
static ssize_t ext2_vnode_read(struct ofile *fd, void *buf, size_t count) {
    struct vnode *vn = fd->vnode;
    struct ext2_inode *inode = (struct ext2_inode *) vn->fs_data;
    struct ext2_extsb *sb = vn->fs->fs_private;

    size_t nread = MIN(inode->size_lower - fd->pos, count);

    if (nread == 0) {
        return -1;
    }

    size_t block_number = fd->pos / sb->block_size;
    size_t nblocks = (nread + sb->block_size - 1) / sb->block_size;
    char block_buffer[sb->block_size];

    for (size_t i = 0; i < nblocks; ++i) {
        if (ext2_read_inode_block(vn->fs, inode, i + block_number, block_buffer) < 0) {
            kerror("Failed to read inode %d block #%zu\n", vn->ino, i + block_number);
            return -EIO;
        }
        if (i == 0) {
            size_t ncpy = MIN(sb->block_size - fd->pos % sb->block_size, nread);
            memcpy(buf, block_buffer + fd->pos % sb->block_size, ncpy);
        } else {
            size_t ncpy = MIN(sb->block_size, nread - sb->block_size * i);
            memcpy((void *) (((uintptr_t) buf) + sb->block_size * i), block_buffer, ncpy);
        }
    }

    return nread;
}

static ssize_t ext2_vnode_write(struct ofile *fd, const void *buf, size_t count) {
    struct vnode *vn = fd->vnode;
    _assert(vn);
    struct ext2_inode *inode = (struct ext2_inode *) vn->fs_data;
    struct fs *ext2 = vn->fs;
    struct ext2_extsb *sb = (struct ext2_extsb *) ext2->fs_private;
    char block_buffer[sb->block_size];
    int res;

    if (fd->pos > inode->size_lower) {
        // This shouldn't be possible, yeah?
        return -ESPIPE;
    }

    // How many bytes can we write into the blocks already allocated
    size_t size_blocks = (inode->size_lower + sb->block_size - 1) / sb->block_size;
    size_t can_write = size_blocks * sb->block_size - inode->size_lower;
    size_t current_block = fd->pos / sb->block_size;
    size_t written = 0;
    size_t remaining = count;

    if (can_write) {
        size_t can_write_blocks = (can_write + sb->block_size - 1) / sb->block_size;

        for (size_t i = 0; i < can_write_blocks; ++i) {
            size_t block_index = current_block + i;
            size_t pos_in_block = fd->pos % sb->block_size;
            size_t need_write = MIN(remaining, sb->block_size - pos_in_block);

            kdebug("Write %zuB to block %zu offset %zu\n", need_write, block_index, pos_in_block);
            if (need_write == sb->block_size) {
                // Can write block without reading it
                // TODO: implement this
                panic("Not implemented\n");
            } else {
                // Read the block to change its contents
                // and write it back again
                if ((res = ext2_read_inode_block(ext2, inode, block_index, block_buffer)) < 0) {
                    break;
                }

                memcpy(block_buffer + pos_in_block, (void *) (((uintptr_t) buf) + written), need_write);

                if ((res = ext2_write_inode_block(ext2, inode, block_index, block_buffer)) < 0) {
                    break;
                }
            }

            written += need_write;
            fd->pos += need_write;
            remaining -= need_write;
        }

        inode->size_lower = MAX(fd->pos, inode->size_lower);
        current_block += can_write_blocks;
    }

    if (remaining) {
        // Need to allocate additional blocks
        size_t need_blocks = (remaining + sb->block_size - 1) / sb->block_size;

        for (size_t i = 0; i < need_blocks; ++i) {
            size_t block_index = current_block + i;
            size_t need_write = MIN(remaining, sb->block_size);

            // Update the size here so it gets written when the block is allocated
            // and inode struct is flushed
            inode->size_lower += need_write;
            // Allocate a block for the index
            if ((res = ext2_inode_alloc_block(ext2, inode, vn->ino, block_index)) < 0) {
                kerror("Could not allocate a block for writing\n");
                break;
            }


            if (need_write == sb->block_size) {
                // TODO: implement this
                panic("Not implemented\n");
            } else {
                // Writing the last block
                memcpy(block_buffer, (void *) (((uintptr_t) buf) + written), need_write);

                if ((res = ext2_write_inode_block(ext2, inode, block_index, block_buffer)) < 0) {
                    break;
                }
            }

            written += need_write;
            fd->pos += need_write;
            remaining -= need_write;
        }
    } else {
        if (written) {
            // Flush inode struct to disk - size has changed
            ext2_write_inode(ext2, inode, vn->ino);
        }
    }

    return written;
}

static int ext2_vnode_truncate(struct vnode *vn, size_t length) {
    _assert(vn);
    struct fs *ext2 = vn->fs;
    struct ext2_inode *inode = (struct ext2_inode *) vn->fs_data;
    struct ext2_extsb *sb = vn->fs->fs_private;
    int res;

    if (length == inode->size_lower) {
        // Already good
        return 0;
    }

    size_t was_blocks = (inode->size_lower + sb->block_size - 1) / sb->block_size;
    size_t now_blocks = (length + sb->block_size - 1) / sb->block_size;
    ssize_t delta_blocks = now_blocks - was_blocks;

    if (delta_blocks < 0) {
        // Free truncated blocks
        // XXX: reverse the loop
        for (size_t i = now_blocks; i < was_blocks; ++i) {
            // Modify inode right here because ext2_free_inode_block will
            // flush these changes to disk so we don't have to write it
            // twice
            inode->size_lower -= sb->block_size;
            if ((res = ext2_free_inode_block(ext2, inode, vn->ino, i)) < 0) {
                // Put the block size back, couldn't free it
                inode->size_lower += sb->block_size;
                return res;
            }
        }

        // All the blocks were successfully freed, can set proper file length
        if (inode->size_lower != length) {
            // If requested size is not block-aligned, we need to write inode
            // struct to disk once again
            inode->size_lower = length;

            if ((res = ext2_write_inode(ext2, inode, vn->ino)) < 0) {
                return res;
            }
        }

        return 0;
    } else {
        kerror("Not implemented: upwards truncation (ext2)\n");
        return -EINVAL;
    }
}

// TODO: replace this with getdents
static ssize_t ext2_vnode_readdir(struct ofile *fd, struct dirent *vfsdir) {
    struct vnode *vn = fd->vnode;
    struct ext2_inode *inode = (struct ext2_inode *) vn->fs_data;
    struct ext2_extsb *sb = vn->fs->fs_private;
    ssize_t res;

    if (fd->pos >= inode->size_lower) {
        return -1;
    }

    size_t block_number = fd->pos / sb->block_size;
    char block_buffer[sb->block_size];

    if ((res = ext2_read_inode_block(vn->fs, inode, block_number, block_buffer)) < 0) {
        return res;
    }

    size_t block_offset = fd->pos % sb->block_size;
    struct ext2_dirent *ext2dir = (struct ext2_dirent *) &block_buffer[block_offset];

    if (ext2dir->len == 0) {
        // If entry size is zero, guess we're finished - align the fd->pos up to block size
        fd->pos = (fd->pos + sb->block_size - 1) / sb->block_size;
        return ext2_vnode_readdir(fd, vfsdir);
    }

    vfsdir->d_ino = ext2dir->ino;
    strncpy(vfsdir->d_name, ext2dir->name, ext2dir->name_len);
    vfsdir->d_name[ext2dir->name_len] = 0;
    vfsdir->d_reclen = ext2dir->len;
    if (sb->required_features & 2 /* Directory entries contain type field */) {
        switch (ext2dir->type_ind) {
        case 1:
            // Regular file
            vfsdir->d_type = DT_REG;
            break;
        case 2:
            // Directory
            vfsdir->d_type = DT_DIR;
            break;
        // XXX: Don't know if I should have ANY devices in ext2, we have devfs for that
        //      kind of shit
        case 0:
        default:
            vfsdir->d_type = DT_UNKNOWN;
            break;
        }
    } else {
        // Have to read each fucking inode to sort this mess out
        struct ext2_inode ent_inode_buf;

        if ((res = ext2_read_inode(vn->fs, &ent_inode_buf, ext2dir->ino)) < 0) {
            return res;
        }

        switch (ent_inode_buf.type_perm & 0xF000) {
        case EXT2_TYPE_REG:
            vfsdir->d_type = DT_REG;
            break;
        case EXT2_TYPE_DIR:
            vfsdir->d_type = DT_DIR;
            break;
        default:
            vfsdir->d_type = DT_UNKNOWN;
            break;
        }
    }
    // Not implemented, I guess
    vfsdir->d_off = 0;

    fd->pos += ext2dir->len;

    return ext2dir->len;
}

//static void ext2_vnode_destroy(vnode_t *vn) {
//    // Release inode struct
//    kfree(vn->fs_data);
//}

static int ext2_vnode_stat(struct vnode *vn, struct stat *st) {
    _assert(vn && vn->fs);
    struct ext2_inode *inode = (struct ext2_inode *) vn->fs_data;
    _assert(inode);
    struct ext2_extsb *sb = (struct ext2_extsb *) vn->fs->fs_private;
    _assert(sb);

    st->st_atime = inode->atime;
    st->st_ctime = inode->ctime;
    st->st_mtime = inode->mtime;
    st->st_dev = 0;     // Not implemented
    st->st_rdev = 0;    // Not implemented
    st->st_gid = inode->gid;
    st->st_uid = inode->uid;
    st->st_mode = inode->type_perm;
    st->st_size = inode->size_lower;
    st->st_blocks = (inode->size_lower + sb->block_size - 1) / sb->block_size;
    st->st_blksize = sb->block_size;
    st->st_nlink = 0;
    st->st_ino = vn->ino;

    return 0;
}

//static int ext2_vnode_chmod(vnode_t *vn, mode_t mode) {
//    _assert(vn && vn->fs && vn->fs_data);
//    struct ext2_inode *inode = (struct ext2_inode *) vn->fs_data;
//
//    // Update only access mode
//    inode->type_perm &= ~0x1FF;
//    inode->type_perm |= mode & 0x1FF;
//
//    // Write the inode back
//    return ext2_write_inode(vn->fs, inode, vn->fs_number);
//}
//
//static int ext2_vnode_chown(vnode_t *vn, uid_t uid, gid_t gid) {
//    _assert(vn && vn->fs && vn->fs_data);
//    struct ext2_inode *inode = (struct ext2_inode *) vn->fs_data;
//
//    inode->gid = gid;
//    inode->uid = uid;
//
//    // Write the inode back
//    return ext2_write_inode(vn->fs, inode, vn->fs_number);
//}
//
//static int ext2_vnode_unlink(vnode_t *at, vnode_t *vn, const char *name) {
//    struct ext2_inode *inode = vn->fs_data;
//    struct ext2_inode *at_inode = at->fs_data;
//    fs_t *ext2 = vn->fs;
//    struct ext2_extsb *sb = (struct ext2_extsb *) ext2->fs_private;
//    uint32_t ino = vn->fs_number;
//    int res;
//
//    if (vn->type == VN_DIR) {
//        // Check if the directory we're unlinking has any entries besides
//        // . and ..
//        // Can tell this just by looking at the first block
//        if (inode->size_lower > sb->block_size) {
//            // Directory size is more than one block - totally
//            // has something inside
//            return -EISDIR;
//        }
//        char block_buffer[sb->block_size];
//        size_t off = 0;
//
//        if ((res = ext2_read_inode_block(ext2, inode, 0, block_buffer)) < 0) {
//            return res;
//        }
//
//        while (off < sb->block_size) {
//            struct ext2_dirent *ent = (struct ext2_dirent *) &block_buffer[off];
//            if (!ent->ino) {
//                break;
//            }
//            if (ent->name_len == 1 && ent->name[0] == '.') {
//                off += ent->len;
//                continue;
//            }
//            if (ent->name_len == 2 && ent->name[1] == '.' && ent->name[0] == '.') {
//                off += ent->len;
//                continue;
//            }
//
//            return -EISDIR;
//        }
//    }
//
//    // Free blocks used by the inode - truncate the file to zero
//    size_t nblocks = (inode->size_lower + sb->block_size - 1) / sb->block_size;
//
//    inode->size_lower = nblocks * sb->block_size;
//    for (ssize_t i = nblocks - 1; i >= 0; --i) {
//        inode->size_lower -= sb->block_size;
//        if ((res = ext2_free_inode_block(ext2, inode, ino, i)) < 0) {
//            return res;
//        }
//    }
//
//    // inode->size_lower is now 0
//    _assert(inode->size_lower == 0);
//
//    // Free the inode itself
//    if ((res = ext2_free_inode(ext2, ino)) < 0) {
//        return res;
//    }
//
//    // Now remove the entry from directory
//    if ((res = ext2_dir_remove_inode(ext2, at, name, ino)) < 0) {
//        return res;
//    }
//
//    return 0;
//}
//
//static int ext2_vnode_access(vnode_t *vn, uid_t *uid, gid_t *gid, mode_t *mode) {
//    _assert(vn && vn->fs_data);
//    struct ext2_inode *inode = vn->fs_data;
//
//    *uid = inode->uid;
//    *gid = inode->gid;
//    *mode = inode->type_perm & 0x1FF;
//
//    return 0;
//}
//
static int ext2_vnode_readlink(struct vnode *vn, char *dst, size_t lim) {
    _assert(vn && vn->fs_data);
    struct ext2_inode *inode = vn->fs_data;
    struct fs *ext2 = vn->fs;
    struct ext2_extsb *sb = (struct ext2_extsb *) ext2->fs_private;

    if (lim <= inode->size_lower) {
        return -1;
    }

    if (inode->size_lower >= 60) {
        char block_buffer[sb->block_size];
        int res;

        if ((res = ext2_read_inode_block(ext2, inode, 0, block_buffer)) < 0) {
            return res;
        }

        strncpy(dst, block_buffer, inode->size_lower);
        dst[inode->size_lower] = 0;
    } else {
        const char *src = (const char *) inode->direct_blocks;
        strncpy(dst, src, inode->size_lower);
        dst[inode->size_lower] = 0;
    }

    return 0;
}
//
//static int ext2_vnode_symlink(vnode_t *at, struct vfs_ioctx *ctx, const char *name, const char *dst) {
//    _assert(at && at->fs && at->fs_data);
//    struct ext2_inode *inode = at->fs_data;
//    fs_t *ext2 = at->fs;
//    struct ext2_extsb *sb = (struct ext2_extsb *) ext2->fs_private;
//
//    uint32_t new_ino;
//    int res;
//
//    // Allocate new inode number
//    if ((res = ext2_alloc_inode(ext2, &new_ino)) != 0) {
//        kerror("Failed to allocate inode\n");
//        return res;
//    }
//
//    kdebug("Allocated inode %d\n", new_ino);
//
//    // Create an inode struct in memory
//    struct ext2_inode *ent_inode = (struct ext2_inode *) kmalloc(sb->inode_struct_size);
//
//    // Now create an entry in parents dirent list
//    if ((res = ext2_dir_add_inode(ext2, at, name, new_ino)) < 0) {
//        return res;
//    }
//
//    // Fill the inode
//    ent_inode->flags = 0;
//    ent_inode->dir_acl = 0;
//    ent_inode->frag_block_addr = 0;
//    ent_inode->gen_number = 0;
//    ent_inode->hard_link_count = 1;
//    ent_inode->acl = 0;
//    ent_inode->os_value_1 = 0;
//    memset(ent_inode->os_value_2, 0, sizeof(ent_inode->os_value_2));
//    // TODO: time support in kernel
//    ent_inode->atime = 0;
//    ent_inode->mtime = 0;
//    ent_inode->ctime = 0;
//    ent_inode->dtime = 0;
//
//    ent_inode->size_lower = strlen(dst);
//
//    if (ent_inode->size_lower <= 60) {
//        char *dst_str = (char *) ent_inode->direct_blocks;
//        memset(dst_str, 0, 60);
//
//        strncpy(dst_str, dst, ent_inode->size_lower);
//    } else {
//        char block_buffer[sb->block_size];
//        uint32_t block_no;
//
//        if ((res = ext2_alloc_block(ext2, &block_no)) < 0) {
//            return res;
//        }
//
//        memset(block_buffer, 0, sb->block_size);
//        strncpy(block_buffer, dst, sb->block_size);
//
//        if ((res = ext2_write_block(ext2, block_no, block_buffer)) < 0) {
//            return res;
//        }
//
//        memset(ent_inode->direct_blocks, 0, sizeof(ent_inode->direct_blocks));
//        ent_inode->l1_indirect_block = 0;
//        ent_inode->l2_indirect_block = 0;
//        ent_inode->l3_indirect_block = 0;
//
//        ent_inode->direct_blocks[0] = block_no;
//    }
//
//    ent_inode->uid = ctx->uid;
//    ent_inode->gid = ctx->gid;
//    ent_inode->type_perm = 0777 | EXT2_TYPE_LNK;
//
//    // Write the inode
//    if ((res = ext2_write_inode(ext2, ent_inode, new_ino)) < 0) {
//        return res;
//    }
//
//    return 0;
//}

static off_t ext2_vnode_lseek(struct ofile *fd, off_t offset, int whence) {
    _assert(fd);
    struct vnode *node = fd->vnode;
    _assert(node);
    _assert(node->type == VN_REG);
    struct ext2_inode *inode = node->fs_data;
    _assert(inode);

    size_t max = inode->size_lower;

    switch (whence) {
    case SEEK_SET:
        if ((size_t) offset > max) {
            // Don't support seeking beyond file end
            return -ENXIO;
        }
        fd->pos = offset;
        break;
    default:
        panic("Unsupported whence: %d\n", whence);
    }

    return fd->pos;
}
