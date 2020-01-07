Yggdrasil x86-64 kernel
======================================

(I still need refactoring, huh.)

Yggdrasil is the name for my kernel development project which 
tries to follow the Unix principles (sheesh, yet another POSIX 
OS?) and is loosely inspired by stuff like Linux,
BSD and other *nixes I've laid my hands on so far.

![alt text](doc/demo0.png "Demo")

Building
--------

```Shell session
    cp defconfig config
    export ARCH=amd64
    make
```

The results are build/sys/amd64/loader.elf (32-bit loader and for 64-bit kernel)
and build/sys/amd64/kernel.elf (the kernel itself)

What works
----------

* Multithreading (kernel/user)
* kill()/signal()
* VFS implementation with non-resident ("mapper") filesystem support
* ext2 driver supporting both read/write, directories and symlinks
* VT100 basic control sequences for color/cursor control in tty.
* ELF binary loading
* fork()/exec()
* ARP name resolution

What works: amd64-specific
--------------------------

* SMP (though I haven't tested it well):
    - APs are started up using InitIPI/SIPIs
    - LAPIC config
    - LAPIC timer for each processor
    - Per-CPU scheduling queues
* I/O APIC configuration:
    - 8259 PIC -> I/O APIC mapping
    - Trigger/polarity config from MADT table (for 8259 IRQs)
* PCI:
    - Device enumeration
    - PCI-to-PCI bridge support
    - Resolve INTA#/INTB#/INTC#/INTD# pin routing
* ACPI:
    - Integrated ACPICA into the kernel. Singlethreaded-only so far.
    - PCI Interrupt Link Devices (\_SB_.LNKx/GSIx) for dynamic PCI IRQ mapping
    - Hard IRQ routes (for chipsets where there's no IRQ router and PCI traces are hardwired)
    - Shutdown works (Sleep state 5)

What works: devices
-------------------

* RTL8139 Network Controller - both Tx/Rx
* AHCI SATA controllers (R/O)
* IDE controllers (R/O)

Credits
-------

* Linux for default8x16 font
