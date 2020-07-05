#include "user/fcntl.h"
#include "user/errno.h"
#include "arch/amd64/cpu.h"
#include "arch/amd64/mm/phys.h"
#include "arch/amd64/mm/pool.h"
#include "fs/sysfs.h"
#include "sys/snprintf.h"
#include "fs/ofile.h"
#include "fs/node.h"
#include "sys/assert.h"
#include "sys/string.h"
#include "sys/thread.h"
#include "sys/mem/slab.h"
#include "fs/fs.h"
#include "sys/mod.h"
#include "sys/debug.h"
#include "sys/heap.h"
#include "sys/attr.h"

static int sysfs_init(struct fs *fs, const char *opt);
static struct vnode *sysfs_get_root(struct fs *fs);
static int sysfs_vnode_open(struct ofile *fd, int opt);
static void sysfs_vnode_close(struct ofile *fd);
static ssize_t sysfs_vnode_read(struct ofile *fd, void *buf, size_t count);
static int sysfs_vnode_stat(struct vnode *node, struct stat *st);

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
    .write = NULL,
    .stat = sysfs_vnode_stat,
};

////

static void sysfs_ensure_root(void) {
    if (!g_sysfs_root) {
        g_sysfs_root = vnode_create(VN_DIR, NULL);
        g_sysfs_root->flags |= VN_MEMORY;
        g_sysfs_root->mode = 0755;
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
    if ((opt & O_ACCMODE) != O_RDONLY) {
        return -1;
    }

    _assert(fd->file.vnode);
    struct sysfs_config_endpoint *endp = fd->file.vnode->fs_data;
    int res;
    _assert(endp);

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
    _assert(fd->file.priv_data);
    kfree(fd->file.priv_data);
    fd->file.priv_data = 0;
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

int sysfs_add_dir(struct vnode *at, const char *name, struct vnode **res) {
    struct vnode *tmp;
    sysfs_ensure_root();
    if (!at) {
        at = g_sysfs_root;
    }
    // TODO: proper check that "at" vnode belongs to sysfs
    _assert(at->flags & VN_MEMORY);

    if (vnode_lookup_child(at, name, &tmp) != 0) {
        kdebug("Can't find %s in %s, creating\n", name, at->name);
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

// TODO: move this to an appropriate place
#define PROC_PROP_PID           1
#define PROC_PROP_NAME          2

static int proc_property_getter(void *ctx, char *buf, size_t lim) {
    uint64_t prop = (uint64_t) ctx;
    struct thread *thr = get_cpu()->thread;
    _assert(thr);

    switch (prop) {
    case PROC_PROP_PID:
        sysfs_buf_printf(buf, lim, "%d\n", (int) thr->pid);
        break;
    case PROC_PROP_NAME:
        sysfs_buf_puts(buf, lim, thr->name);
        sysfs_buf_puts(buf, lim, "\n");
        break;
    }

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
    struct amd64_phys_stat phys_st;
    struct heap_stat heap_st;
    struct amd64_pool_stat pool_st;
    struct slab_stat slab_st;

    slab_stat(&slab_st);
    amd64_phys_stat(&phys_st);
    amd64_mm_pool_stat(&pool_st);
    heap_stat(heap_global, &heap_st);

    sysfs_buf_printf(buf, lim, "PhysTotal:      %u kB\n", phys_st.limit / 1024);
    sysfs_buf_printf(buf, lim, "PhysFree:       %u kB\n", phys_st.pages_free * 4);
    sysfs_buf_printf(buf, lim, "PhysUsed:       %u kB\n", phys_st.pages_used * 4);

    sysfs_buf_printf(buf, lim, "PoolTotal:      %u kB\n", MM_POOL_SIZE / 1024);
    sysfs_buf_printf(buf, lim, "PoolFree:       %u kB\n", pool_st.pages_free * 4);
    sysfs_buf_printf(buf, lim, "PoolUsed:       %u kB\n", pool_st.pages_used * 4);

    sysfs_buf_printf(buf, lim, "HeapTotal:      %u kB\n", heap_st.total_size / 1024);
    sysfs_buf_printf(buf, lim, "HeapFree:       %u kB\n", heap_st.free_size / 1024);
    sysfs_buf_printf(buf, lim, "HeapUsed:       %u kB\n", (heap_st.total_size - heap_st.free_size) / 1024);

    sysfs_buf_printf(buf, lim, "SlabObjects:    %u\n",    slab_st.alloc_objects);
    sysfs_buf_printf(buf, lim, "SlabTotal:      %u kB\n", slab_st.alloc_pages * 4);
    sysfs_buf_printf(buf, lim, "SlabUsed:       %u kB\n", slab_st.alloc_bytes / 1024);

    return 0;
}

void sysfs_populate(void) {
    struct vnode *dir;
    sysfs_add_dir(NULL, "kernel", &dir);

    sysfs_add_config_endpoint(dir, "version", SYSFS_MODE_DEFAULT, sizeof(KERNEL_VERSION_STR) + 1, KERNEL_VERSION_STR, sysfs_config_getter, NULL);
    sysfs_add_config_endpoint(dir, "uptime", SYSFS_MODE_DEFAULT, 32, NULL, system_uptime_getter, NULL);
    sysfs_add_config_endpoint(dir, "modules", SYSFS_MODE_DEFAULT, 512, NULL, mod_list, NULL);

    sysfs_add_dir(NULL, "self", &dir);
    sysfs_add_config_endpoint(dir, "pid", SYSFS_MODE_DEFAULT, 16, (void *) PROC_PROP_PID, proc_property_getter, NULL);
    sysfs_add_config_endpoint(dir, "name", SYSFS_MODE_DEFAULT, 128, (void *) PROC_PROP_NAME, proc_property_getter, NULL);

    sysfs_add_config_endpoint(NULL, "mem", SYSFS_MODE_DEFAULT, 512, NULL, system_mem_getter, NULL);
}

static void __init sysfs_class_init(void) {
    fs_class_register(&g_sysfs);
}
