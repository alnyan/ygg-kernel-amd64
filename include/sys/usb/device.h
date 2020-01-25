#pragma once
#include "sys/types.h"
#include "sys/usb/request.h"

struct usb_transfer;
struct usb_request;

struct usb_device {
    void *hc;

    uint16_t address;
    uint8_t speed;
    uint32_t max_packet;

    uint8_t endpoint_toggle;
    struct usb_desc_interface desc_interface;
    struct usb_desc_endpoint desc_endpoint;
    struct usb_driver *driver;
    void *driver_data;

    void (*hc_control)(struct usb_device *, struct usb_transfer *);
    void (*hc_interrupt)(struct usb_device *, struct usb_transfer *);

    struct usb_device *next;
};

struct usb_device *usb_device_create(void);
int usb_device_init(struct usb_device *dev);
int usb_device_request(struct usb_device *dev, uint8_t type, uint8_t request, uint16_t index, uint16_t value, uint16_t len, void *data);
