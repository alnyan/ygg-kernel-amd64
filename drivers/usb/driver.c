#include "drivers/usb/driver.h"
#include "sys/types.h"

static struct usb_driver *g_driver_head = NULL;

int usb_select_driver(struct usb_device *dev) {
    for (struct usb_driver *drv = g_driver_head; drv; drv = drv->next) {
        if (drv->usb_init && drv->usb_init(dev) == 0) {
            return 0;
        }
    }

    return -1;
}

void usb_add_driver(struct usb_driver *drv) {
    drv->next = g_driver_head;
    g_driver_head = drv;
}
