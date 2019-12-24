Just another attempt at OS development
======================================

I need refactoring, huh.

What works
----------

* SMP:
    - APs are started up using InitIPI/SIPIs
    - LAPIC config
    - LAPIC timer for each processor
    - Per-CPU scheduling queues
* Multitasking
* Userspace stuff with a simple libc
* Readonly (yet) AHCI and IDE ATA controller drivers
* I/O APIC configuration:
    - 8259 PIC -> I/O APIC mapping
    - Trigger/polarity config from MADT table (for 8259 IRQs)
* PCI:
    - Device enumeration
    - PCI-to-PCI bridge support
    - Resolve INTA#/INTB#/INTC#/INTD# pin routing
* ACPI:
    - Integrated ACPICA into the kernel. Only singlethreaded access, synchronization not implemented yet
    - PCI Interrupt Link Devices (\_SB_.LNKx/GSIx) for dynamic PCI IRQ mapping
    - Hard IRQ routes (for chipsets where there's no IRQ router and PCI traces are hardwired)
    - Shutdown works (Sleep state 5)
