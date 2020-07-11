#include "drivers/usb/const.h"
#include "drivers/usb/device.h"
#include "drivers/usb/request.h"
#include "drivers/usb/driver.h"
#include "sys/assert.h"
#include "sys/debug.h"
#include "sys/heap.h"
#include "sys/attr.h"

#define USB_DESCRIPTOR_HUB              0x29

#define USB_FEATURE_PORT_RESET          4
#define USB_FEATURE_PORT_POWER          8

#define HUB_STATUS_PORT_CONNECTED       (1 << 0)
#define HUB_STATUS_PORT_ENABLED         (1 << 1)
#define HUB_STATUS_PORT_SUSPENDED       (1 << 2)
#define HUB_STATUS_PORT_SPEED_SHIFT     9
#define HUB_STATUS_PORT_SPEED_MASK      (0x3 << HUB_STATUS_PORT_SPEED_SHIFT)

#define HUB_POWER_MASK                  0x03
#define HUB_POWER_INDIVIDUAL            0x01

struct usb_desc_hub {
    uint8_t len;
    uint8_t type;
    uint8_t port_count;
    uint16_t chars;
    uint8_t power_time;
    uint8_t current;
} __attribute__((packed));

struct usb_hub {
    struct usb_desc_hub desc_hub;
};

static void usb_hub_poll(struct usb_device *dev);
static int usb_hub_init(struct usb_device *dev);

static struct usb_driver g_usb_hub = {
    .usb_init = usb_hub_init,
    .usb_poll = usb_hub_poll
};

static void usb_hub_poll(struct usb_device *dev) {
}

static uint16_t usb_hub_reset_port(struct usb_device *dev, uint8_t port) {
    int res;

    if ((res = usb_device_request(dev,
                                  USB_REQUEST_TYPE_CLASS | USB_REQUEST_TYPE_OTHER,
                                  USB_REQUEST_SET_FEATURE,
                                  port + 1,
                                  USB_FEATURE_PORT_RESET,
                                  0, 0)) != 0) {
        kwarn("Hub port reset failed\n");
        return 0;
    }

    uint32_t status = 0;

    for (int attempt = 0; attempt < 10; ++attempt) {
        // Wait for port to reset
        for (int i = 0; i < 1000000; ++i) {
            asm volatile ("pause");
        }

        // Get status
        if ((res = usb_device_request(dev,
                                      USB_REQUEST_TYPE_CLASS | USB_REQUEST_TYPE_D2H | USB_REQUEST_TYPE_OTHER,
                                      USB_REQUEST_GET_STATUS,
                                      port + 1,
                                      0,
                                      sizeof(status), &status)) != 0) {
            kwarn("Failed to get hub status after port reset\n");
            return 0;
        }

        if (!(status & HUB_STATUS_PORT_CONNECTED)) {
            break;
        }

        if (status & HUB_STATUS_PORT_ENABLED) {
            break;
        }
    }

    return status;
}

static int usb_hub_probe(struct usb_device *dev) {
    struct usb_hub *hub = dev->driver_data;
    uint8_t port_count = hub->desc_hub.port_count;
    int res;

    // Check if each port has separate power controls
    if ((hub->desc_hub.chars & HUB_POWER_MASK) == HUB_POWER_INDIVIDUAL) {
        // Enable all the ports when probing
        for (uint8_t i = 0; i < port_count; ++i) {
            if ((res = usb_device_request(dev,
                                          USB_REQUEST_TYPE_CLASS | USB_REQUEST_TYPE_OTHER,
                                          USB_REQUEST_SET_FEATURE,
                                          i + 1,
                                          USB_FEATURE_PORT_POWER,
                                          0, 0)) != 0) {
                kwarn("Port power on failed for hub\n");
                return res;
            }

            // Wait for port to power on
            for (int i = 0; i < 1000000; ++i) {
                asm volatile ("pause");
            }
        }
    }

    for (uint8_t i = 0; i < port_count; ++i) {
        // Reset port
        uint16_t status = usb_hub_reset_port(dev, i);

        if (status & HUB_STATUS_PORT_ENABLED) {
            struct usb_device *ndev = usb_device_create();
            ndev->hc = dev->hc;
            ndev->max_packet = 8;
            ndev->hc_control = dev->hc_control;
            ndev->hc_interrupt = dev->hc_interrupt;
            ndev->address = 0;
            ndev->endpoint_toggle = 0;
            ndev->speed = (status & HUB_STATUS_PORT_SPEED_MASK) >> HUB_STATUS_PORT_SPEED_SHIFT;

            usb_device_init(ndev);
        }
    }

    return 0;
}

static int usb_hub_init(struct usb_device *dev) {
    int res;

    if (dev->desc_interface.interface_class == USB_CLASS_HUB) {
        struct usb_hub *hub = kmalloc(sizeof(struct usb_hub));
        _assert(hub);

        dev->driver = &g_usb_hub;
        dev->driver_data = hub;

        // Get hub descriptor
        if ((res = usb_device_request(dev,
                                      USB_REQUEST_TYPE_D2H | USB_REQUEST_TYPE_CLASS,
                                      USB_REQUEST_GET_DESCRIPTOR,
                                      0,
                                      USB_DESCRIPTOR_HUB << 8,
                                      sizeof(struct usb_desc_hub),
                                      &hub->desc_hub)) != 0) {
            kwarn("Failed to get hub descriptor\n");
            return res;
        }

        kinfo("Hub descriptor:\n");
        kinfo("Port count: %d\n", hub->desc_hub.port_count);
        kinfo("Characteristics: %02x\n", hub->desc_hub.chars);
        kinfo("Power time: %d\n", hub->desc_hub.power_time);
        kinfo("Current: %d\n", hub->desc_hub.current);

        usb_hub_probe(dev);

        return 0;
    }

    return -1;
}

__init(usb_hub_register_driver) {
    usb_add_driver(&g_usb_hub);
}
