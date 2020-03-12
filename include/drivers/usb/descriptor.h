#pragma once
#include "sys/types.h"

#define USB_DESCRIPTOR_DEVICE           1
#define USB_DESCRIPTOR_CONFIGURATION    2
#define USB_DESCRIPTOR_STRING           3
#define USB_DESCRIPTOR_INTERFACE        4
#define USB_DESCRIPTOR_ENDPOINT         5

struct usb_desc_configuration {
    uint8_t length;
    uint8_t type;
    uint16_t total_length;
    uint8_t num_interfaces;
    uint8_t configuration_value;
    uint8_t idx_configuration;
    uint8_t attributes;
    uint8_t max_power;
} __attribute__((packed));

struct usb_desc_interface {
    uint8_t length;
    uint8_t type;
    uint8_t interface_number;
    uint8_t alternate_setting;
    uint8_t num_endpoints;
    uint8_t interface_class;
    uint8_t interface_subclass;
    uint8_t interface_protocol;
    uint8_t idx_interface;
} __attribute__((packed));

struct usb_desc_endpoint {
    uint8_t length;
    uint8_t type;
    uint8_t address;
    uint8_t attributes;
    uint16_t max_packet_size;
    uint8_t interval;
} __attribute__((packed));

struct usb_desc_string {
    uint8_t len;
    uint8_t type;
    uint16_t data[0];
} __attribute__((packed));

struct usb_desc_device {
    uint8_t length;
    uint8_t type;
    uint16_t bcd_usb;
    uint8_t dev_class;
    uint8_t dev_subclass;
    uint8_t dev_protocol;
    uint8_t max_packet_zero;
    // After 8 bytes
    uint16_t id_vendor;
    uint16_t id_product;
    uint16_t bcd_device;
    uint8_t idx_manufacturer;
    uint8_t idx_product;
    uint8_t idx_serial_number;
    uint8_t num_configurations;
} __attribute__((packed));
