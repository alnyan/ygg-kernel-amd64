#include "sys/driver/pci/pci.h"

static const char *pci_class_code_strings[] = {
    "Unclassified",
    "Mass storage controller",
    "Network controller",
    "Display controller",
    "Multimedia controller",
    "Memory controller",
    "Bridge device",
    "Simple communication controller",
    "Base system peripheral",
    "Input device controller",
    "Docking station",
    "Processor",
    "Serial bus controller",
    "Wireless controller",
    "Intelligent controller",
    "Satellite communication controller",
    "Encryption controller",
    "Signal processing controller",
    "Processing accelerator",
    "Non-essential instrumentation"
};

const char *pci_class_string(uint16_t full_class) {
    // Better descriptions for well-known classes
    switch (full_class) {
    // Mass storage
    case 0x0101:
        return "IDE controller";
    case 0x0106:
        return "SATA controller";
    // Display controllers
    case 0x0300:
        return "VGA display controller";
    // Network controllers
    case 0x200:
        return "Ethernet controller";
    // Bridge devices
    case 0x0600:
        return "Host bridge";
    case 0x0601:
        return "ISA bridge";
    // Serial bus controllers
    case 0x0C03:
        return "USB controller";
    case 0x0C05:
        return "SMBus";
    default:
        break;
    }

    uint8_t class_code = full_class >> 8;
    if (class_code == 0xFF) {
        return "Unknown device";
    }

    if (class_code >= sizeof(pci_class_code_strings) / sizeof(pci_class_code_strings[0])) {
        return "Reserved";
    }

    return pci_class_code_strings[class_code];
}

