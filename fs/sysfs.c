#include "user/fcntl.h"
#include "user/errno.h"
#include "arch/amd64/mm/pool.h"
#include "fs/sysfs.h"
#include "sys/snprintf.h"
#include "fs/ofile.h"
#include "fs/node.h"
#include "sys/assert.h"
#include "sys/string.h"
#include "sys/thread.h"
#include "sys/mem/slab.h"
#include "sys/mem/phys.h"
#include "fs/fs.h"
#include "sys/mod.h"
#include "sys/debug.h"
#include "sys/config.h"
#include "sys/heap.h"
#include "sys/attr.h"

static int sysfs_init(struct fs *fs, const char *opt);
static struct vnode *sysfs_get_root(struct fs *fs);
static int sysfs_vnode_open(struct ofile *fd, int opt);
static void sysfs_vnode_close(struct ofile *fd);
static ssize_t sysfs_vnode_read(struct ofile *fd, void *buf, size_t count);
static ssize_t sysfs_vnode_write(struct ofile *fd, const void *buf, size_t count);
static int sysfs_vnode_stat(struct vnode *node, struct stat *st);
static int sysfs_vnode_truncate(struct vnode *node, size_t size) {
    return 0;
}

struct sysfs_config_endpoint {
    size_t bufsz;
    void *ctx;
    cfg_read_func_t read;
    cfg_write_func_t write;
};

////

static struct vnode *g_sysfs_root = NULL;
static struct fs_class g_sysfs = {
    .name = "sysfs",
    .opt = 0,
    .init = sysfs_init,
    .get_root = sysfs_get_root
};
static struct vnode_operations g_sysfs_vnode_ops = {
    .open = sysfs_vnode_open,
    .close = sysfs_vnode_close,
    .read = sysfs_vnode_read,
    .write = sysfs_vnode_write,
    .stat = sysfs_vnode_stat,
    .truncate = sysfs_vnode_truncate
};

////

static void sysfs_ensure_root(void) {
    if (!g_sysfs_root) {
        g_sysfs_root = vnode_create(VN_DIR, NULL);
        g_sysfs_root->flags |= VN_MEMORY;
        g_sysfs_root->mode = 0755;
        g_sysfs_root->op = &g_sysfs_vnode_ops;
    }
}

static int sysfs_init(struct fs *fs, const char *opt) {
    sysfs_ensure_root();
    return 0;
}

static struct vnode *sysfs_get_root(struct fs *fs) {
    _assert(g_sysfs_root);
    return g_sysfs_root;
}

////

static int sysfs_vnode_open(struct ofile *fd, int opt) {
    _assert(fd->file.vnode);
    struct sysfs_config_endpoint *endp = fd->file.vnode->fs_data;
    int res;
    _assert(endp);

    switch (opt & O_ACCMODE) {
    case O_WRONLY:
        if (!endp->write) {
            return -EPERM;
        }
        break;
    case O_RDONLY:
        if (!endp->read) {
            return -EPERM;
        }
        break;
    case O_RDWR:
        if (!endp->read || !endp->write) {
            return -EPERM;
        }
        break;
    }

    fd->file.priv_data = kmalloc(endp->bufsz);
    if (!fd->file.priv_data) {
        return -ENOMEM;
    }
    ((char *) fd->file.priv_data)[0] = 0;
    fd->file.pos = 0;

    if (endp->read) {
        if ((res = endp->read(endp->ctx, (char *) fd->file.priv_data, endp->bufsz)) != 0) {
            kfree(fd->file.priv_data);
            return res;
        }
    }

    return 0;
}

static void sysfs_vnode_close(struct ofile *fd) {
    if (fd->file.priv_data) {
        kfree(fd->file.priv_data);
        fd->file.priv_data = 0;
    }
}

static ssize_t sysfs_vnode_read(struct ofile *fd, void *buf, size_t count) {
    _assert(fd);
    const char *data = fd->file.priv_data;
    _assert(data);
    size_t max = strlen(data) + 1;

    if (fd->file.pos >= max) {
        return 0;
    }

    size_t r = MIN(max - fd->file.pos, count);
    memcpy(buf, data + fd->file.pos, r);

    fd->file.pos += r;

    return r;
}

static ssize_t sysfs_vnode_write(struct ofile *fd, const void *buf, size_t count) {
    _assert(fd);
    _assert(fd->file.vnode);
    struct sysfs_config_endpoint *endp = fd->file.vnode->fs_data;
    // TODO: use count
    size_t len = strlen(buf);
    int res;
    _assert(endp);

    if (!endp->write) {
        return -EPERM;
    }

    if (len >= endp->bufsz) {
        return -ENOMEM;
    }

    if ((res = endp->write(endp->ctx, buf)) != 0) {
        return res;
    }

    strcpy(fd->file.priv_data, buf);

    return len;
}

static int sysfs_vnode_stat(struct vnode *node, struct stat *st) {
    st->st_size = 0;
    st->st_blksize = 0;
    st->st_blocks = 0;
    st->st_dev = 0;
    st->st_rdev = 0;
    st->st_ctime = system_boot_time;
    st->st_mtime = 0;
    st->st_atime = 0;
    st->st_nlink = 1;

    if (node->fs_data) {
        st->st_mode = S_IFREG;
    } else {
        st->st_mode = S_IFDIR;
    }
    st->st_mode |= node->mode & 0xFFF;
    st->st_uid = node->uid;
    st->st_gid = node->gid;

    return 0;
}

////

int sysfs_config_getter(void *ctx, char *buf, size_t lim) {
    sysfs_buf_puts(buf, lim, ctx);
    sysfs_buf_puts(buf, lim, "\n");
    return 0;
}

int sysfs_config_int64_getter(void *ctx, char *buf, size_t lim) {
    // TODO: long format for snprintf
    sysfs_buf_printf(buf, lim, "%d\n", *(int *) ctx);
    return 0;
}

int sysfs_del_ent(struct vnode *ent) {
    _assert(ent);
    _assert(ent->parent);
    vnode_detach(ent);

    switch (ent->type) {
    case VN_LNK:
        break;
    case VN_REG:
        _assert(ent->op == &g_sysfs_vnode_ops);
        _assert(ent->fs_data);
        kfree(ent->fs_data);
        break;
    case VN_DIR:
        while (ent->first_child) {
            sysfs_del_ent(ent->first_child);
        }
        break;
    default:
        panic("Unsupported sysfs node type\n");
        break;
    }

    vnode_destroy(ent);
    return 0;
}

int sysfs_add_dir(struct vnode *at, const char *name, struct vnode **res) {
    struct vnode *tmp;
    sysfs_ensure_root();
    if (!at) {
        at = g_sysfs_root;
    }
    // TODO: proper check that "at" vnode belongs to sysfs
    _assert(at->flags & VN_MEMORY);

    if (vnode_lookup_child(at, name, &tmp) != 0) {
        tmp = vnode_create(VN_DIR, name);
        tmp->flags |= VN_MEMORY;
        tmp->op = &g_sysfs_vnode_ops;
        tmp->mode = S_IXUSR | S_IXGRP | S_IXOTH |
                    S_IRUSR | S_IRGRP | S_IROTH;

        vnode_attach(at, tmp);
    }

    *res = tmp;
    return 0;
}

int sysfs_add_config_endpoint(struct vnode *at, const char *name, mode_t mode, size_t bufsz, void *ctx, cfg_read_func_t read, cfg_write_func_t write) {
    struct sysfs_config_endpoint *endp = kmalloc(sizeof(struct sysfs_config_endpoint));
    _assert(endp);

    endp->bufsz = bufsz;
    endp->ctx = ctx;
    endp->read = read;
    endp->write = write;

    struct vnode *node = vnode_create(VN_REG, name);
    node->flags |= VN_MEMORY;
    node->mode = mode;
    // Clear permission bits for unsupported operations
    if (!write) {
        node->mode &= ~(S_IWUSR | S_IWOTH | S_IWGRP);
    }
    if (!read) {
        node->mode &= ~(S_IRUSR | S_IROTH | S_IRGRP);
    }
    // Cannot be executable
    node->mode &= ~(S_IXUSR | S_IXOTH | S_IXGRP);
    node->op = &g_sysfs_vnode_ops;
    node->fs_data = endp;

    sysfs_ensure_root();
    if (!at) {
        at = g_sysfs_root;
    }
    // TODO: proper check that "at" vnode belongs to sysfs
    _assert(at->flags & VN_MEMORY);
    vnode_attach(at, node);

    return 0;
}

static int system_uptime_getter(void *ctx, char *buf, size_t lim) {
    char *p = buf;
    uint64_t t = system_time / 1000000000ULL;
    int days = (t / 86400),
        hours = (t / 3600) % 24,
        minutes = (t / 60) % 60,
        seconds = t % 60;

    sysfs_buf_printf(buf, lim, "%u day", days);
    if (days != 1) {
        sysfs_buf_puts(buf, lim, "s");
    }
    sysfs_buf_printf(buf, lim, ", %02u:%02u:%02u\n", hours, minutes, seconds);

    return 0;
}

static int system_mem_getter(void *ctx, char *buf, size_t lim) {
    struct mm_phys_stat phys_st;
    struct heap_stat heap_st;
    struct slab_stat slab_st;

    slab_stat(&slab_st);
    mm_phys_stat(&phys_st);
    heap_stat(heap_global, &heap_st);

    sysfs_buf_printf(buf, lim, "PhysTotal:      %u kB\n", phys_st.pages_total * 4);
    sysfs_buf_printf(buf, lim, "PhysFree:       %u kB\n", phys_st.pages_free * 4);
    sysfs_buf_printf(buf, lim, "UsedKernel:     %u kB\n", phys_st.pages_used_kernel * 4);
    sysfs_buf_printf(buf, lim, "UsedUser:       %u kB\n", phys_st.pages_used_user * 4);
    sysfs_buf_printf(buf, lim, "UsedShared:     %u kB\n", phys_st.pages_used_shared * 4);
    sysfs_buf_printf(buf, lim, "UsedPaging:     %u kB\n", phys_st.pages_used_paging * 4);
    sysfs_buf_printf(buf, lim, "UsedCache:      %u kB\n", phys_st.pages_used_cache * 4);

    sysfs_buf_printf(buf, lim, "HeapTotal:      %u kB\n", heap_st.total_size / 1024);
    sysfs_buf_printf(buf, lim, "HeapFree:       %u kB\n", heap_st.free_size / 1024);
    sysfs_buf_printf(buf, lim, "HeapUsed:       %u kB\n", (heap_st.total_size - heap_st.free_size) / 1024);

    sysfs_buf_printf(buf, lim, "SlabObjects:    %u\n",    slab_st.alloc_objects);
    sysfs_buf_printf(buf, lim, "SlabTotal:      %u kB\n", slab_st.alloc_pages * 4);
    sysfs_buf_printf(buf, lim, "SlabUsed:       %u kB\n", slab_st.alloc_bytes / 1024);

    return 0;
}

static int debug_config_get(void *ctx, char *buf, size_t lim) {
    if (!strcmp(ctx, "serial")) {
        sysfs_buf_printf(buf, lim, "%u\n", kernel_config[CFG_DEBUG] & 0xFF);
        return 0;
    } else if (!strcmp(ctx, "display")) {
        sysfs_buf_printf(buf, lim, "%u\n", (kernel_config[CFG_DEBUG] >> 8) & 0xFF);
        return 0;
    } else {
        return -EINVAL;
    }
}

static int debug_config_set(void *ctx, const char *buf) {
    // TODO: length param to setters
    int level = atoi(buf);
    if (!strcmp(ctx, "serial")) {
        kernel_config[CFG_DEBUG] &= ~0xFF;
        kernel_config[CFG_DEBUG] |= level & 0xFF;
        return 0;
    } else if (!strcmp(ctx, "display")) {
        kernel_config[CFG_DEBUG] &= ~0xFF00;
        kernel_config[CFG_DEBUG] |= (level & 0xFF) << 8;
        return 0;
    } else {
        return -EINVAL;
    }
}

void sysfs_populate(void) {
    struct vnode *dir;
    sysfs_add_dir(NULL, "kernel", &dir);

    sysfs_add_config_endpoint(dir, "version", SYSFS_MODE_DEFAULT, sizeof(KERNEL_VERSION_STR) + 1, KERNEL_VERSION_STR, sysfs_config_getter, NULL);
    sysfs_add_config_endpoint(dir, "uptime", SYSFS_MODE_DEFAULT, 32, NULL, system_uptime_getter, NULL);
    sysfs_add_config_endpoint(dir, "modules", SYSFS_MODE_DEFAULT, 512, NULL, mod_list, NULL);
    sysfs_add_config_endpoint(dir, "debug_serial", SYSFS_MODE_DEFAULT, 32, "serial", debug_config_get, debug_config_set);
    sysfs_add_config_endpoint(dir, "debug_display", SYSFS_MODE_DEFAULT, 32, "display", debug_config_get, debug_config_set);

    sysfs_add_config_endpoint(NULL, "mem", SYSFS_MODE_DEFAULT, 512, NULL, system_mem_getter, NULL);

}

__init(sysfs_class_init) {
    fs_class_register(&g_sysfs);
}
