#include "sys/amd64/sys_sys.h"
#include "sys/amd64/hw/rtc.h"
#include "sys/amd64/cpu.h"
#include "sys/reboot.h"
#include "sys/assert.h"
#include "sys/errno.h"
#include "sys/sched.h"

int sys_mount(const char *dev_name, const char *dir_name, const char *type, unsigned long flags, void *data) {
    struct thread *thr = get_cpu()->thread;
    struct vnode *dev_node;
    void *dev;
    int res;
    _assert(thr);
    _assert(dir_name);

    if (thr->ioctx.uid != 0) {
        // Only root can do that
        return -EACCES;
    }

    if (dev_name) {
        if ((res = vfs_find(&thr->ioctx, thr->ioctx.cwd_vnode, dev_name, &dev_node)) != 0) {
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

    return vfs_mount(&thr->ioctx, dir_name, dev, type, NULL);
}

int sys_nanosleep(const struct timespec *req, struct timespec *rem) {
    struct thread *cur_thread = get_cpu()->thread;
    _assert(cur_thread);

    uint64_t time_now = system_time;
    uint64_t deadline = time_now + req->tv_sec * 1000000000ULL + req->tv_nsec;

    // Make sure scheduler won't select a waiting thread
    while (1) {
        // TODO: interruptible sleep
        if (system_time >= deadline) {
            break;
        }
        asm volatile ("sti; hlt; cli");
    }

    return 0;
}

int sys_gettimeofday(struct timeval *tv, struct timezone *tz) {
    if (tz) {
        tz->tz_dsttime = 0;
        tz->tz_minuteswest = 0;
    }

    uint64_t secs = system_time / 1000000000ULL;

    struct rtc_time time_rtc;
    rtc_read(&time_rtc);
    secs += time_rtc.second + time_rtc.minute * 60 + time_rtc.hour * 3600;

    // System time is in nanos
    // TODO: use RTC-provided time for full system time
    tv->tv_usec = (system_time / 1000) % 1000000;
    tv->tv_sec = secs;

    return 0;
}

int sys_reboot(int magic1, int magic2, unsigned int cmd, void *arg) {
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

