#include "sys/mem/phys.h"
#include "fs/ram_tar.h"
#include "user/errno.h"
#include "sys/string.h"
#include "sys/assert.h"
#include "sys/debug.h"
#include "fs/node.h"
#include "fs/vfs.h"
#include "fs/ram.h"
#include "sys/mm.h"
#include "fs/fs.h"

#include <stdbool.h>

struct tar_header {
    char name[100];
    char mode[8];
    char uid[8];
    char gid[8];
    char size[12];
    char mtime[12];
    char checksum[8];
    char typeflag[1];
    char linkname[100];
    char magic[6];
    char version[2];
    char uname[32];
    char gname[32];
    char devmajor[8];
    char devminor[8];
    char prefix[155];
    char pad[12];
} __attribute__((packed)) /* I don't know if gcc actually decides to reorder something */;

static ssize_t tar_octal(const char *buf, size_t lim) {
    ssize_t res = 0;
    for (size_t i = 0; i < lim; ++i) {
        if (!buf[i]) {
            break;
        }

        res <<= 3;
        res |= buf[i] - '0';
    }
    return res;
}

static const char *path_element(const char *path, char *element) {
    const char *sep = strchr(path, '/');
    if (!sep) {
        strcpy(element, path);
        return NULL;
    } else {
        _assert(sep - path < NODE_MAXLEN);
        strncpy(element, path, sep - path);
        element[sep - path] = 0;

        while (*sep == '/') {
            ++sep;
        }
        if (!*sep) {
            return NULL;
        }

        return sep;
    }
}

static int tar_mapper_create_path(struct vnode *root,
                                  const char *path,
                                  struct vnode **res,
                                  enum vnode_type type,
                                  int create) {
    char name[NODE_MAXLEN];
    const char *child_path = path;
    struct vnode *node = root;
    struct vnode *new_node;
    int err;

    while (1) {
        child_path = path_element(child_path, name);

        // Node doesn't yet exist
        // TODO: check early that the node CAN contain children
        //       i.e. is a directory?
        if (vnode_lookup_child(node, name, &new_node) != 0) {
            if (!create) {
                return -ENOENT;
            }
            new_node = ram_vnode_create(type, name);
            if (!new_node) {
                return -ENOMEM;
            }

            vnode_attach(node, new_node);
        }

        node = new_node;

        if (!child_path) {
            break;
        }
    }

    *res = node;

    return 0;
}

static enum vnode_type tar_type(struct tar_header *hdr) {
    enum vnode_type vn_type = VN_UNK;
    switch (hdr->typeflag[0]) {
    case 0:
    case '0':
        vn_type = VN_REG;
        break;
    case '2':
        vn_type = VN_LNK;
        break;
    case '5':
        vn_type = VN_DIR;
        break;
    }

    _assert(vn_type != VN_UNK);
    return vn_type;
}

int tar_init(struct fs *ramfs, void *mem_base) {
    size_t off = 0;
    char *bytes;
    struct tar_header *hdr;
    enum vnode_type type;
    struct vnode *node;
    bool prev_zero = 0;
    ssize_t res;

    size_t file_length, file_block_count;

    // First pass builds the filesystem tree
    while (1) {
        bytes = (char *) mem_base + off;

        if (!bytes[0]) {
            if (prev_zero) {
                break;
            }
            prev_zero = 1;
            off += 512;
            continue;
        }

        hdr = (struct tar_header *) bytes;
        type = tar_type(hdr);

        // Create node
        if ((res = tar_mapper_create_path(ramfs->fs_private,
                                          hdr->name,
                                          &node,
                                          type,
                                          1)) != 0) {
            return res;
        }

        if (type == VN_REG) {
            file_length = tar_octal(hdr->size, sizeof(hdr->size));
            off += 512 + ((511 + file_length) & ~511);
        } else {
            off += 512;
        }
    }

    // Second pass:
    // 1. Setups regular file blocks for work with RAMFS
    // 2. Resolves (if possible) symbolic links
    off = 0;
    prev_zero = 0;
    while (1) {
        bytes = (char *) mem_base + off;

        if (!bytes[0]) {
            if (prev_zero) {
                break;
            }
            prev_zero = 1;
            off += 512;
            continue;
        }

        hdr = (struct tar_header *) bytes;
        type = tar_type(hdr);

        // TODO: a function dedicated to looking up the node by its path
        //       might want something like vfs_find here, but it requires
        //       ioctx
        if ((res = tar_mapper_create_path(ramfs->fs_private,
                                          hdr->name,
                                          &node,
                                          0, 0)) != 0) {
            return res;
        }

        // Extract node attributes
        node->uid = tar_octal(hdr->uid, sizeof(hdr->uid));
        node->gid = tar_octal(hdr->gid, sizeof(hdr->uid));
        node->mode = tar_octal(hdr->mode, sizeof(hdr->mode)) & VFS_MODE_MASK;

        if (type == VN_LNK) {
            file_length = strlen(hdr->linkname);
            uintptr_t blk = mm_phys_alloc_page();
            _assert(blk != MM_NADDR);
            if ((res = ram_vnode_bset_resize(node, file_length)) != 0) {
                panic("Failed to resize file\n");
            }
            _assert(ram_vnode_bset_set(node, 0, MM_VIRTUALIZE(blk)) == 0);
            strcpy((void *) MM_VIRTUALIZE(blk), hdr->linkname);

            if (hdr->linkname[0] == '/') {
                if ((res = tar_mapper_create_path(ramfs->fs_private,
                                                  hdr->linkname + 1,
                                                  &node->target,
                                                  0, 0)) != 0) {
                    panic("Broken symlink in initrd: '%s' -> '%s'\n", hdr->name, hdr->linkname);
                }
            } else {
                if ((res = tar_mapper_create_path(node,
                                                  hdr->linkname,
                                                  &node->target,
                                                  0, 0)) != 0) {
                    panic("Broken symlink in initrd: '%s' -> '%s'\n", hdr->name, hdr->linkname);
                }

            }
        } else if (type == VN_REG) {
            file_length = tar_octal(hdr->size, sizeof(hdr->size));
            file_block_count = (511 + file_length) / 512;

            if ((res = ram_vnode_bset_resize(node, file_length)) != 0) {
                panic("Failed to resize file\n");
            }

            // Set the blocks
            for (size_t i = 0; i < file_block_count; ++i) {
                _assert(ram_vnode_bset_set(node, i, (uintptr_t) bytes + (i * 512) + 512) == 0);
            }

            off += file_block_count * 512;
        }

        off += 512;
    }

    return 0;
}
