#include "sys/amd64/hw/usb/device.h"
#include "sys/amd64/hw/usb/request.h"
#include "sys/string.h"
#include "sys/assert.h"
#include "sys/debug.h"
#include "sys/heap.h"

static struct usb_device *g_usb_devices = NULL;
static uint32_t g_device_address = 0;

struct usb_device *usb_device_create(void) {
    struct usb_device *res = kmalloc(sizeof(struct usb_device));
    _assert(res);

    res->hc_control = NULL;
    res->next = g_usb_devices;
    g_usb_devices = res;

    return res;
}

int usb_device_request(struct usb_device *dev, uint8_t type, uint8_t request, uint16_t index, uint16_t value, uint16_t len, void *data) {
    _assert(dev);
    struct usb_request req;
    req.request = request;
    req.type = type;
    req.value = value;
    req.index = index;
    req.length = len;

    struct usb_transfer t;
    t.request = &req;
    t.data = data;
    t.length = len;
    t.flags = 0;

    _assert(dev->hc_control);
    dev->hc_control(dev, &t);

    return !(t.flags & USB_TRANSFER_SUCCESS);
}

static int usb_device_get_langs(struct usb_device *dev, uint16_t *langs, uint16_t lim) {
    char buf[256];
    struct usb_desc_string *desc;
    int res;

    langs[0] = 0;
    desc = (struct usb_desc_string *) buf;

    // Get string length
    if ((res = usb_device_request(dev,
                                  USB_REQUEST_TYPE_D2H,
                                  USB_REQUEST_GET_DESCRIPTOR,
                                  0,
                                  USB_DESCRIPTOR_STRING << 8,
                                  1, desc)) != 0) {
        kwarn("Failed to get string descriptor length\n");
        return res;
    }

    if (((desc->len - 2) / 2) >= lim) {
        kwarn("Not enough buffer space to store device language data\n");
        return -1;
    }

    // Get lang data
    if ((res = usb_device_request(dev,
                                  USB_REQUEST_TYPE_D2H,
                                  USB_REQUEST_GET_DESCRIPTOR,
                                  0,
                                  USB_DESCRIPTOR_STRING << 8,
                                  desc->len, desc))) {
        kwarn("Failed to get string descriptor\n");
        return res;
    }

    uint16_t lang_len = (desc->len - 2) / 2;
    uint16_t i;
    for (i = 0; i < lang_len; ++i) {
        langs[i] = desc->data[i];
    }
    langs[i] = 0;

    return 0;
}

static int usb_device_get_string(struct usb_device *dev, char *buf, size_t lim, uint16_t lang, uint16_t index) {
    buf[0] = 0;
    if (!index) {
        return 0;
    }

    char _buf[256];
    struct usb_desc_string *desc = (struct usb_desc_string *) _buf;
    int res;

    if ((res = usb_device_request(dev,
                                  USB_REQUEST_TYPE_D2H,
                                  USB_REQUEST_GET_DESCRIPTOR,
                                  lang,
                                  (USB_DESCRIPTOR_STRING << 8) | index,
                                  1, desc)) != 0) {
        kwarn("Failed to get string length\n");
        return res;
    }

    size_t len = (desc->len - 2) / 2;
    // Check that buffer has enough space to fit the string
    if (lim <= len) {
        return -1;
    }

    if ((res = usb_device_request(dev,
                                  USB_REQUEST_TYPE_D2H,
                                  USB_REQUEST_GET_DESCRIPTOR,
                                  lang,
                                  (USB_DESCRIPTOR_STRING << 8) | index,
                                  desc->len, desc)) != 0) {
        kwarn("Failed to get string length\n");
        return res;
    }

    // Fuck unicode, we're only doing ASCII here
    for (size_t i = 0; i < len; ++i) {
        buf[i] = (char) desc->data[i];
    }
    buf[len] = 0;

    return len;
}

int usb_device_init(struct usb_device *dev) {
    struct usb_desc_device device_desc;
    char vendor_name[256];
    char product_name[256];
    uint16_t langs[32];
    uint32_t dev_addr;
    int res;

    kinfo("Getting partial descriptor\n");
    if ((res = usb_device_request(dev,
                                  USB_REQUEST_TYPE_D2H,
                                  USB_REQUEST_GET_DESCRIPTOR,
                                  0,
                                  USB_DESCRIPTOR_DEVICE << 8,
                                  8, &device_desc)) != 0) {
        kwarn("Failed to get device descriptor\n");
        return res;
    }

    dev->max_packet = device_desc.max_packet_zero;

    // Allocate a new address for the device
    dev_addr = ++g_device_address;

    kinfo("Assigning new address\n");
    if ((res = usb_device_request(dev,
                                  0,
                                  USB_REQUEST_SET_ADDRESS,
                                  0,
                                  dev_addr,
                                  0, 0))) {
        kwarn("Failed to set device address\n");
        return res;
    }
    dev->address = dev_addr;

    // Wait a bit
    for (size_t _ = 0; _ < 100000; ++_);

    kinfo("Reading full descriptor\n");
    memset(&device_desc, 0, sizeof(struct usb_desc_device));
    // Read full device descriptor
    if ((res = usb_device_request(dev,
                                  USB_REQUEST_TYPE_D2H,
                                  USB_REQUEST_GET_DESCRIPTOR,
                                  0,
                                  USB_DESCRIPTOR_DEVICE << 8,
                                  sizeof(struct usb_desc_device), &device_desc)) != 0) {
        kwarn("Failed to get full device descriptor\n");
        return res;
    }

    kinfo("Specification version: %04x\n", device_desc.bcd_usb);
    kinfo("Device class: %02x:%02x\n", device_desc.dev_class, device_desc.dev_subclass);
    kinfo("Protocol: %02x\n", device_desc.dev_protocol);
    kinfo("Device ID: %04x:%04x\n", device_desc.id_vendor, device_desc.id_product);

    // Dump detailed info
    if ((res = usb_device_get_langs(dev, langs, sizeof(langs) / sizeof(langs[0]))) != 0) {
        return res;
    }

    if (langs[0]) {
        uint16_t lang = langs[0];

        if ((res = usb_device_get_string(dev, vendor_name, sizeof(vendor_name), lang, device_desc.idx_manufacturer)) <= 0) {
            kwarn("Failed to get device vendor name\n");
        } else {
            kinfo("Vendor name: %s\n", vendor_name);
        }

        if ((res = usb_device_get_string(dev, product_name, sizeof(product_name), lang, device_desc.idx_product)) <= 0) {
            kwarn("Failed to get device product name\n");
        } else {
            kinfo("Product name: %s\n", product_name);
        }
    } else {
        kwarn("No language data for device\n");
    }

    return 0;
}
