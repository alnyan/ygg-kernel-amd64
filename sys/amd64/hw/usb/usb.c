#include "sys/amd64/hw/usb/device.h"
#include "sys/amd64/hw/usb/driver.h"
#include "sys/amd64/hw/usb/usb.h"
#include "sys/assert.h"
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

void usb_controller_add(struct usb_controller *hc) {
    hc->next = g_hc_list;
    g_hc_list = hc;
}
