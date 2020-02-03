#pragma once
#include "sys/types.h"

#define USB_SPEC_UHCI       0x00

#define USB_SPEED_LOW       0x00
#define USB_SPEED_FULL      0x01
#define USB_SPEED_HIGH      0x02

struct usb_controller {
    uint8_t spec;
    void (*hc_poll)(struct usb_controller *hc);

    struct usb_controller *next;
};

void usb_controller_add(struct usb_controller *hc);
void usb_daemon_start(void);
void usb_poll(void);
