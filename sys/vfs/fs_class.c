#include "sys/string.h"
#include "sys/fs/fs.h"
#include "sys/errno.h"
#include "sys/mem.h"

// #include <stddef.h>
// #include <string.h>
// #include <errno.h>

static struct fs_class *fses[10] = { NULL };
static struct fs mounts[10];

struct fs *fs_create(struct fs_class *cls, struct blkdev *blk, vnode_t *at) {
    for (size_t i = 0; i < 10; ++i) {
        if (mounts[i].cls == NULL) {
            mounts[i].cls = cls;
            mounts[i].blk= blk;
            mounts[i].mnt_at = at;
            return &mounts[i];
        }
    }
    return NULL;
}

struct fs_class *fs_class_by_name(const char *name) {
    for (size_t i = 0; i < 10; ++i) {
        if (!fses[i]) {
            break;
        }
        if (!strcmp(name, fses[i]->name)) {
            return fses[i];
        }
    }
    return NULL;
}

int fs_class_register(struct fs_class *cls) {
    if (fs_class_by_name(cls->name)) {
        return -EEXIST;
    }

    for (size_t i = 0; i < 10; ++i) {
        if (!fses[i]) {
            fses[i] = cls;
            return 0;
        }
    }

    return -ENOMEM;
}
