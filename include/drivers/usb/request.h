#pragma once
#include "sys/types.h"

#define USB_REQUEST_TYPE_D2H            (1 << 7)
#define USB_REQUEST_TYPE_CLASS          (1 << 5)
#define USB_REQUEST_TYPE_OTHER          0x03

#define USB_REQUEST_TO_INTERFACE        (1 << 0)

// General
#define USB_REQUEST_GET_STATUS          0
#define USB_REQUEST_SET_FEATURE         3
#define USB_REQUEST_SET_ADDRESS         5
#define USB_REQUEST_GET_DESCRIPTOR      6
#define USB_REQUEST_SET_CONFIGURATION   9

// USB HID
#define USB_REQUEST_SET_IDLE            0x0A

struct usb_request {
    uint8_t type;
    uint8_t request;
    uint16_t value;
    uint16_t index;
    uint16_t length;
};

#define USB_TRANSFER_COMPLETE           (1 << 0)
#define USB_TRANSFER_SUCCESS            (1 << 1)

struct usb_transfer {
    struct usb_request *request;
    uint8_t endp;
    void *data;
    size_t length;
    uint32_t flags;
};
