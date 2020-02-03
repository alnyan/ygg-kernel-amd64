#include "sys/driver/usb/device.h"
#include "sys/driver/usb/driver.h"
#include "sys/driver/usb/usb.h"
#include "sys/amd64/cpu.h"
#include "sys/thread.h"
#include "sys/assert.h"
#include "sys/sched.h"
#include "sys/debug.h"
#include "sys/heap.h"

static struct usb_controller *g_hc_list = NULL;
static struct usb_device *g_usb_devices = NULL;

struct usb_device *usb_device_create(void) {
    struct usb_device *res = kmalloc(sizeof(struct usb_device));
    _assert(res);

    res->hc_control = NULL;
    res->next = g_usb_devices;
    res->driver = NULL;
    g_usb_devices = res;

    return res;
}

void usb_poll(void) {
    for (struct usb_controller *hc = g_hc_list; hc; hc = hc->next) {
        if (hc->hc_poll) {
            hc->hc_poll(hc);
        }
    }

    for (struct usb_device *dev = g_usb_devices; dev; dev = dev->next) {
        if (dev->driver && dev->driver->usb_poll) {
            dev->driver->usb_poll(dev);
        }
    }
}

static void *usb_daemon(void *arg) {
    kinfo("USB daemon started\n");

    while (1) {
        usb_poll();
        // Sleep for 10ms
        thread_sleep(thread_self, system_time + 10000000ULL, NULL);
    }

    panic("This code should not run\n");
}

void usb_daemon_start(void) {
    static struct thread usbd_thread;
    if (!g_hc_list) {
        kinfo("Not starting USB daemon - no HCs\n");
        // No controllers to poll
        return;
    }

    _assert(thread_init(&usbd_thread, (uintptr_t) usb_daemon, NULL, 0) == 0);
    usbd_thread.pid = thread_alloc_pid(0);
    sched_queue(&usbd_thread);
}

void usb_controller_add(struct usb_controller *hc) {
    hc->next = g_hc_list;
    g_hc_list = hc;
}
