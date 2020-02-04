#include "drivers/usb/driver.h"
#include "drivers/usb/device.h"
#include "drivers/usb/const.h"
#include "sys/char/input.h"
#include "sys/char/tty.h"
#include "sys/string.h"
#include "sys/assert.h"
#include "sys/debug.h"
#include "sys/attr.h"
#include "sys/heap.h"

#define USB_KBD_MOD_SHIFT       ((1 << 1) | (1 << 5))
#define USB_KBD_MOD_CONTROL     ((1 << 0) | (1 << 4))
#define USB_KBD_CAPS            0x39

static int usb_kbd_init(struct usb_device *dev);
static void usb_kbd_poll(struct usb_device *dev);

static const char usb_kbd_ascii_0[256] = {
    0,
    [0x04] = 'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h',
    [0x0C] = 'i', 'j', 'k', 'l', 'm', 'n', 'o', 'p',
    [0x14] = 'q', 'r', 's', 't', 'u', 'v', 'w', 'x',
    [0x1C] = 'y', 'z', '1', '2', '3', '4', '5', '6',
    [0x24] = '7', '8', '9', '0',
    [0x28] = '\n', '\033', '\b', '\t',
    [0x2C] = ' ', '-', '=', '[', ']', '\\', '.', ';',
    [0x34] = '\'', '`', ',', '.', '/',
    // ...
    [0x4F] = INPUT_KEY_RIGHT,
    [0x50] = INPUT_KEY_LEFT,
    [0x51] = INPUT_KEY_DOWN,
    [0x52] = INPUT_KEY_UP,
};

static const char usb_kbd_ascii_1[256] = {
    0,
    [0x04] = 'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H',
    [0x0C] = 'I', 'J', 'K', 'L', 'M', 'N', 'O', 'P',
    [0x14] = 'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X',
    [0x1C] = 'Y', 'Z', '!', '@', '#', '$', '%', '^',
    [0x24] = '&', '*', '(', ')',
    [0x28] = '\n', '\033', '\b', '\t',
    [0x2C] = ' ', '_', '+', '{', '}', '|', '.', ':',
    [0x34] = '"', '~', '<', '>', '?',
    // ...
    [0x4F] = INPUT_KEY_RIGHT,
    [0x50] = INPUT_KEY_LEFT,
    [0x51] = INPUT_KEY_DOWN,
    [0x52] = INPUT_KEY_UP,
};

struct usb_kbd_data {
    struct usb_transfer transfer;
    uint8_t mods;
    char buf[8];
    char keys_down[6];
};

static struct usb_driver g_usbkbd = {
    .usb_init = usb_kbd_init,
    .usb_poll = usb_kbd_poll
};

static void usb_kbd_handle_data(struct usb_kbd_data *data) {
    data->mods &= ~(INPUT_MOD_CONTROL | INPUT_MOD_SHIFT);
    if (data->buf[0] & USB_KBD_MOD_SHIFT) {
        data->mods |= INPUT_MOD_SHIFT;
    }
    if (data->buf[0] & USB_KBD_MOD_CONTROL) {
        data->mods |= INPUT_MOD_CONTROL;
    }

    for (size_t i = 2; i < 8; ++i) {
        uint8_t key = data->buf[i];

        if (key >= 4) {
            if (!memchr(data->keys_down, data->buf[i], 6)) {
                if (key == 0x39) {
                    data->mods ^= INPUT_MOD_CAPS;
                } else {
                    // Key press detected
                    input_key(data->buf[i], data->mods, usb_kbd_ascii_0, usb_kbd_ascii_1);
                }
            }
        } else if (key) {
            kinfo("Invalid key: %02x\n", key);
        }
    }

    memcpy(data->keys_down, &data->buf[2], 6);
}

static void usb_kbd_poll(struct usb_device *dev) {
    struct usb_kbd_data *data = dev->driver_data;
    _assert(data);
    struct usb_transfer *t = &data->transfer;

    if (t->flags & USB_TRANSFER_COMPLETE) {
        if (t->flags & USB_TRANSFER_SUCCESS) {
            usb_kbd_handle_data(data);
        }

        t->flags = 0;
        t->data = data->buf;
        t->length = 8;
        dev->endpoint_toggle ^= 1;
        _assert(dev->hc_interrupt);
        dev->hc_interrupt(dev, t);
    }
}

static int usb_kbd_init(struct usb_device *dev) {
    int res;
    if (dev->desc_interface.interface_class == USB_CLASS_HID &&
        dev->desc_interface.interface_subclass == USB_SUBCLASS_BOOT &&
        dev->desc_interface.interface_protocol == USB_PROTOCOL_KEYBOARD) {
        kinfo("Found keyboard!\n");

        uint8_t interface_index = dev->desc_interface.idx_interface;

        if ((res = usb_device_request(dev,
                                      USB_REQUEST_TYPE_CLASS | USB_REQUEST_TO_INTERFACE,
                                      0x0A,
                                      interface_index,
                                      0,
                                      0, 0)) != 0) {
            kwarn("USB_REQUEST_SET_IDLE failed\n");
        }

        dev->driver = &g_usbkbd;
        struct usb_kbd_data *data = kmalloc(sizeof(struct usb_kbd_data));
        _assert(data);

        struct usb_transfer *t = &data->transfer;
        t->endp = dev->desc_endpoint.address & 0xF;
        t->request = 0;
        t->data = data->buf;
        t->length = 8;
        t->flags = 0;

        dev->driver_data = data;

        _assert(dev->hc_interrupt);
        dev->hc_interrupt(dev, t);

        return 0;
    }
    return -1;
}

static __init void usb_kbd_register_driver(void) {
    usb_add_driver(&g_usbkbd);
}
