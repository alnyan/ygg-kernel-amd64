#include "user/fcntl.h"
#include "user/errno.h"
#include "arch/amd64/cpu.h"
#include "fs/sysfs.h"
#include "sys/snprintf.h"
#include "fs/ofile.h"
#include "fs/node.h"
#include "sys/assert.h"
#include "sys/string.h"
#include "sys/thread.h"
#include "fs/fs.h"
#include "sys/debug.h"
#include "sys/heap.h"
#include "sys/attr.h"

static int sysfs_init(struct fs *fs, const char *opt);
static struct vnode *sysfs_get_root(struct fs *fs);
static int sysfs_vnode_open(struct ofile *fd, int opt);
static void sysfs_vnode_close(struct ofile *fd);
static ssize_t sysfs_vnode_read(struct ofile *fd, void *buf, size_t count);

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
    .write = NULL
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

    _assert(fd->vnode);
    struct sysfs_config_endpoint *endp = fd->vnode->fs_data;
    int res;
    _assert(endp);

    fd->priv_data = kmalloc(endp->bufsz);
    if (!fd->priv_data) {
        return -ENOMEM;
    }
    ((char *) fd->priv_data)[0] = 0;
    fd->pos = 0;

    if (endp->read) {
        if ((res = endp->read(endp->ctx, (char *) fd->priv_data, endp->bufsz)) != 0) {
            kfree(fd->priv_data);
            return res;
        }
    }

    return 0;
}

static void sysfs_vnode_close(struct ofile *fd) {
    _assert(fd->priv_data);
    kfree(fd->priv_data);
    fd->priv_data = 0;
}

static ssize_t sysfs_vnode_read(struct ofile *fd, void *buf, size_t count) {
    _assert(fd);
    const char *data = fd->priv_data;
    _assert(data);
    size_t max = strlen(data) + 1;

    if (fd->pos >= max) {
        return 0;
    }

    size_t r = MIN(max - fd->pos, count);
    memcpy(buf, data + fd->pos, r);

    fd->pos += r;

    return r;
}

////


int sysfs_config_getter(void *ctx, char *buf, size_t lim) {
    sysfs_buf_puts(buf, lim, ctx);
    sysfs_buf_puts(buf, lim, "\n");
    return 0;
}

int sysfs_config_int64_getter(void *ctx, char *buf, size_t lim) {
    // TODO: long format for snprintf
    //sysfs_buf_printf(buf, lim, "%d\n", *(int *) ctx);
    return 0;
}

int sysfs_add_config_endpoint(const char *name, size_t bufsz, void *ctx, cfg_read_func_t read, cfg_write_func_t write) {
    struct sysfs_config_endpoint *endp = kmalloc(sizeof(struct sysfs_config_endpoint));
    _assert(endp);

    endp->bufsz = bufsz;
    endp->ctx = ctx;
    endp->read = read;
    endp->write = write;

    struct vnode *node = vnode_create(VN_REG, name);
    node->flags |= VN_MEMORY;
    node->mode = 0600;
    node->op = &g_sysfs_vnode_ops;
    node->fs_data = endp;

    sysfs_ensure_root();
    vnode_attach(g_sysfs_root, node);

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

void sysfs_populate(void) {
    sysfs_add_config_endpoint("version", sizeof(KERNEL_VERSION_STR) + 1, KERNEL_VERSION_STR, sysfs_config_getter, NULL);

    sysfs_add_config_endpoint("uptime", 32, NULL, system_uptime_getter, NULL);

    sysfs_add_config_endpoint("self.pid", 16, (void *) PROC_PROP_PID, proc_property_getter, NULL);
    sysfs_add_config_endpoint("self.name", 128, (void *) PROC_PROP_NAME, proc_property_getter, NULL);
}

static void __init sysfs_class_init(void) {
    fs_class_register(&g_sysfs);
}
