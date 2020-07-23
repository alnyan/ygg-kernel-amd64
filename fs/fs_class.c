#include "sys/mem/slab.h"
#include "sys/assert.h"
#include "user/errno.h"
#include "sys/string.h"
#include "sys/debug.h"
#include "fs/fs.h"

static LIST_HEAD(fs_class_list);
static LIST_HEAD(fs_mount_list);

static struct slab_cache *fs_mount_cache;

struct fs *fs_create(struct fs_class *cls, struct blkdev *blk, uint32_t flags, const char *opt) {
    if (!fs_mount_cache) {
        kdebug("Initialize filesystem instance (mount) cache\n");
        fs_mount_cache = slab_cache_get(sizeof(struct fs));
        _assert(fs_mount_cache);
    }
    struct fs *fs = slab_calloc(fs_mount_cache);
    _assert(fs);

    list_head_init(&fs->link);
    fs->cls = cls;
    fs->blk = blk;
    fs->flags = flags;

    if (cls->init) {
        if (cls->init(fs, opt) != 0) {
            kerror("%s: filesystem instance init failed\n", cls->name);
            slab_free(fs_mount_cache, fs);
            return NULL;
        }
    } else {
        kwarn("%s: filesystem type has no init func\n", cls->name);
    }

    list_add(&fs->link, &fs_mount_list);

    return fs;
}

struct fs_class *fs_class_by_name(const char *name) {
    struct fs_class *cls;
    list_for_each_entry(cls, &fs_class_list, link) {
        if (!strcmp(cls->name, name)) {
            return cls;
        }
    }
    return NULL;
}

int fs_class_register(struct fs_class *cls) {
    if (fs_class_by_name(cls->name)) {
        return -EEXIST;
    }

    list_head_init(&cls->link);
    list_add(&cls->link, &fs_class_list);
    return 0;
}
