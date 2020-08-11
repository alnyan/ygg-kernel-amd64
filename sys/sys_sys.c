#include "user/utsname.h"
#include "user/reboot.h"
#include "user/errno.h"
#include "user/time.h"
#include "sys/sys_sys.h"
#include "sys/block/blk.h"
#include "fs/node.h"
#include "sys/string.h"
#include "sys/assert.h"
#include "sys/thread.h"
#include "fs/vfs.h"
#include "sys/sched.h"
#include "sys/debug.h"

int sys_mount(const char *dev_name, const char *dir_name, const char *type, unsigned long flags, void *data) {
    struct process *proc = thread_self->proc;
    struct vnode *dev_node;
    void *dev;
    int res;
    _assert(dir_name);

    if (proc->ioctx.uid != 0) {
        // Only root can do that
        return -EACCES;
    }

    if (dev_name) {
        if ((res = vfs_find(&proc->ioctx, proc->ioctx.cwd_vnode, dev_name, 0, &dev_node)) != 0) {
            return res;
        }

        // Check that it's a block device:
        if (dev_node->type != VN_BLK) {
            return -EINVAL;
        }

        dev = dev_node->dev;
    } else {
        dev = NULL;
    }

    return vfs_mount(&proc->ioctx, dir_name, dev, type, (uint32_t) flags, data);
}

int sys_umount(const char *dir_name) {
    return vfs_umount(&thread_self->proc->ioctx, dir_name);
}

int sys_nanosleep(const struct timespec *req, struct timespec *rem) {
    struct thread *thr = thread_self;
    _assert(thr);
    uint64_t deadline = req->tv_sec * 1000000000ULL + req->tv_nsec + system_time;
    uint64_t int_time;
    int ret = thread_sleep(thr, deadline, &int_time);
    if (rem) {
        if (ret) {
            _assert(deadline > int_time);
            uint64_t rem_time = deadline - int_time;
            rem->tv_sec = rem_time / 1000000000ULL;
            rem->tv_nsec = rem_time % 1000000000ULL;
        } else {
            rem->tv_sec = 0;
            rem->tv_nsec = 0;
        }
    }
    return ret;
}

int sys_gettimeofday(struct timeval *tv, struct timezone *tz) {
    if (tz) {
        tz->tz_dsttime = 0;
        tz->tz_minuteswest = 0;
    }

    uint64_t secs = system_time / 1000000000ULL + system_boot_time;
    // System time is in nanos
    tv->tv_usec = (system_time / 1000) % 1000000;
    tv->tv_sec = secs;

    return 0;
}

int sys_uname(struct utsname *name) {
    if (!name) {
        return -EFAULT;
    }

    strcpy(name->sysname, "yggdrasil");
    // XXX: Hostname is not present in the kernel yet
    strcpy(name->nodename, "nyan");
    // XXX: No release numbers yet, only git version
    strcpy(name->release, "X.Y");
    strcpy(name->version, KERNEL_VERSION_STR);
    // It's the only platform I'm developing the kernel for
    strcpy(name->machine, "x86_64");
    strcpy(name->domainname, "localhost");

    return 0;
}

int sys_reboot(int magic1, int magic2, unsigned int cmd, void *arg) {
    if (thread_self->proc->ioctx.uid != 0) {
        return -EACCES;
    }

    if (magic1 != (int) YGG_REBOOT_MAGIC1 || magic2 != (int) YGG_REBOOT_MAGIC2) {
        return -EINVAL;
    }

    switch (cmd) {
    case YGG_REBOOT_RESTART:
    case YGG_REBOOT_POWER_OFF:
    case YGG_REBOOT_HALT:
        sched_reboot(cmd);
        return 0;
    default:
        return -EINVAL;
    }
}

int sys_sync(void) {
    blk_sync_all();
    return 0;
}
