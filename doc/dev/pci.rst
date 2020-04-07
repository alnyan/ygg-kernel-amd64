PCI
===

Creating a PCI driver
---------------------

PCI driver source code file reside in ``drivers/pci`` directory. Writing a
driver for a device is as simple as just registering a function which will
be triggered once matching device/class is found when enumerating a bus.

Here's a simple "stub" driver for some superficial device and class::

    #include "drivers/pci/pci.h"
    #include "sys/attr.h"

    static void my_class_driver(pci_device_t *dev) { ... }
    static void my_device_driver(pci_device_t *dev) { ... }

    static __init void register_drivers(void) {
        pci_add_class_driver(0x123456, my_class_driver, "my-class-driver");
        pci_add_device_driver(PCI_ID(0x1234, 0x5678), my_device_driver, "my-device-driver");
    }

Weird number ``0x123456`` here repesents ``(class, subclass, prog. if)`` tuple. For
example, for ``(0x0C, 0x03, 0x00)`` (USB UHCI) this value will is ``0x0C0300``. If
prog. if byte is set to ``0xFF``, then all prog. if values of devices are matched.

In driver code, configuration space words can be read and written::

    uint32_t pci_config_read_dword(struct pci_device *dev, uint16_t off);
    void pci_config_write_dword(struct pci_device *dev, uint16_t off, uint32_t val);

And the following ``off`` values are defined:

* ``PCI_CONFIG_ID`` --- device identification
* ``PCI_CONFIG_CMD`` --- device control and status
* ``PCI_CONFIG_CLASS`` --- class, subclass etc.
* ``PCI_CONFIG_BAR(n)`` --- Base Address Registers
* ``PCI_CONFIG_IRQ`` --- IRQ information

For the actual meaning of these configuration space words, see `corresponding osdev wiki
page <https://wiki.osdev.org/PCI>`_
