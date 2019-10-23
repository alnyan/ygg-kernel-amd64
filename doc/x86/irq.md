Full IRQ routing?
=================

External IRQ sources:
* Legacy IRQ lines
* PCI IRQ pins (INTA#/INTB#/INTC#/INTD#)

Each of these is routed to a single GSI (IRQ of I/O APIC),
which in turn is mapped to a single interrupt vector in any of the Local APICs

Mapping legacy IRQ to a handler:
If I/O APIC is available
1. Get 8259 IRQ -> I/O APIC Line mapping
2. Allocate an interrupt vector/reuse shared vector
3. Add a mapping in I/O APIC
4. Add entry to IRQ handlers list
5. Unmask the GSI
Otherwise:
1. Add entry `irq_number` to IRQ handlers list

I/O APIC init:
1. Assign I/O APIC address variable
2. Relocate existing legacy handlers to I/O APIC
