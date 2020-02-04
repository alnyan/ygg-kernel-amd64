#include "drivers/usb/request.h"
#include "drivers/usb/device.h"
#include "drivers/usb/driver.h"
#include "sys/string.h"
#include "sys/assert.h"
#include "sys/debug.h"
#include "sys/heap.h"

static uint32_t g_device_address = 0;

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
    char string_buffer[256];
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


    uint16_t lang = langs[0];
    if (langs[0]) {
        if ((res = usb_device_get_string(dev, string_buffer, sizeof(string_buffer), lang, device_desc.idx_manufacturer)) <= 0) {
            kwarn("Failed to get device vendor name\n");
        } else {
            kinfo("Vendor name: %s\n", string_buffer);
        }

        if ((res = usb_device_get_string(dev, string_buffer, sizeof(string_buffer), lang, device_desc.idx_product)) <= 0) {
            kwarn("Failed to get device product name\n");
        } else {
            kinfo("Product name: %s\n", string_buffer);
        }
    } else {
        kwarn("No language data for device\n");
    }

    // Get device configuration
    char config_buf[256];
    struct usb_desc_configuration *configuration_desc = (struct usb_desc_configuration *) config_buf;
    struct usb_desc_interface *interface_desc = NULL;
    struct usb_desc_endpoint *endpoint_desc = NULL;

    // This stores first valid configuration
    uint8_t picked_config_value = 0;
    struct usb_desc_interface *picked_interface_desc = NULL;
    struct usb_desc_endpoint *picked_endpoint_desc = NULL;

    for (uint32_t conf_index = 0; conf_index < device_desc.num_configurations; ++conf_index) {
        // Get configuration size
        if ((res = usb_device_request(dev,
                                      USB_REQUEST_TYPE_D2H,
                                      USB_REQUEST_GET_DESCRIPTOR,
                                      0,
                                      (USB_DESCRIPTOR_CONFIGURATION << 8) | conf_index,
                                      4, config_buf)) != 0) {
            kwarn("Failed to get configuration header\n");
            continue;
        }

        if (configuration_desc->total_length > sizeof(config_buf)) {
            kwarn("Configuration buffer too small\n");
            continue;
        }

        // Read the rest of data
        if ((res = usb_device_request(dev,
                                      USB_REQUEST_TYPE_D2H,
                                      USB_REQUEST_GET_DESCRIPTOR,
                                      0,
                                      (USB_DESCRIPTOR_CONFIGURATION << 8) | conf_index,
                                      configuration_desc->total_length, config_buf)) != 0) {
            kwarn("Failed to get device configuration\n");
            continue;
        }

        // Dump configuration info
        kinfo("Device %04x, configuration %u:\n", dev->address, conf_index);
        kinfo("Interface count: %u, value: %u\n",
                configuration_desc->num_interfaces, configuration_desc->configuration_value);
        if (usb_device_get_string(dev, string_buffer, sizeof(string_buffer), lang, configuration_desc->idx_configuration) > 0) {
            kinfo("Configuration name: \"%s\"\n", string_buffer);
        }

        // Select a configuration
        void *data = config_buf + configuration_desc->length;
        void *end = config_buf + configuration_desc->total_length;
        kinfo("Configuration data:\n");

        picked_interface_desc = NULL;
        picked_endpoint_desc = NULL;

        while (data < end) {
            uint8_t len = ((uint8_t *) data)[0];
            uint8_t type = ((uint8_t *) data)[1];

            switch (type) {
            case USB_DESCRIPTOR_INTERFACE:
                interface_desc = (struct usb_desc_interface *) data;
                if (!picked_interface_desc) {
                    picked_interface_desc = interface_desc;
                }

                kinfo("* Interface descriptor\n");
                kinfo("Number %02x, %u endpoints\n",
                        interface_desc->interface_number,
                        interface_desc->num_endpoints);
                kinfo("Class: %02x:%02x, Protocol %02x\n",
                        interface_desc->interface_class,
                        interface_desc->interface_subclass,
                        interface_desc->interface_protocol);

                if (usb_device_get_string(dev, string_buffer, sizeof(string_buffer), lang, interface_desc->idx_interface) > 0) {
                    kinfo("Interface name: \"%s\"\n", string_buffer);
                }

                break;
            case USB_DESCRIPTOR_ENDPOINT:
                endpoint_desc = (struct usb_desc_endpoint *) data;
                if (!picked_endpoint_desc) {
                    picked_endpoint_desc = endpoint_desc;
                }
                kinfo("* Endpoint descriptor\n");
                kinfo("Address: %02x\n", endpoint_desc->address);
                switch (endpoint_desc->attributes & 0x3) {
                case 0:
                    kinfo("Control transfers\n");
                    break;
                case 1:
                    kinfo("Isochronous transfers\n");
                    break;
                case 2:
                    kinfo("Bulk transfers\n");
                    break;
                case 3:
                    kinfo("Interrupt transfers\n");
                    break;
                }
                break;
            }

            data += len;
        }

        if (picked_interface_desc && picked_endpoint_desc) {
            picked_config_value = configuration_desc->configuration_value;
            break;
        }
    }

    if (picked_interface_desc && picked_endpoint_desc) {
        kinfo("Configuring device with value %02x\n", picked_config_value);

        if ((res = usb_device_request(dev,
                                      0,
                                      USB_REQUEST_SET_CONFIGURATION,
                                      0,
                                      picked_config_value,
                                      0, 0)) != 0) {
            return res;
        }

        memcpy(&dev->desc_interface, picked_interface_desc, sizeof(struct usb_desc_interface));
        memcpy(&dev->desc_endpoint, picked_endpoint_desc, sizeof(struct usb_desc_endpoint));

        if ((res = usb_select_driver(dev)) != 0) {
            kwarn("Failed to pick driver for the device\n");
        }
    }

    return 0;
}
