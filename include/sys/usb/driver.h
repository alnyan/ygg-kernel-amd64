#pragma once

struct usb_device;

struct usb_driver {
    int (*usb_init)(struct usb_device *dev);
    void (*usb_poll)(struct usb_device *dev);

    struct usb_driver *next;
};

int usb_select_driver(struct usb_device *dev);
void usb_add_driver(struct usb_driver *drv);
