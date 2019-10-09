Local APIC
==========

LAPIC Base MSR
--------------

`IA32_LAPIC_BASE = 0x1B` model-specific register stores information about Local APIC
status and its base address.

Notable LAPIC registers
-----------------------

| Base offset | Name                | Description                               |
|------------:|---------------------|-------------------------------------------|
|        0x20 | IA32_LAPIC_REG_ID   | Local APIC ID register                    |
|        0x80 | IA32_LAPIC_REG_TPR  | Task Priority Register                    |
|        0xB0 | IA32_LAPIC_REG_EOI  | End of interrupt register                 |
|        0xF0 | IA32_LAPIC_REG_SVR  | Spurious Interrupt Vector Register        |
|       0x300 | IA32_LAPIC_REG_ICR0 | Interrupt Control Register, 31 .. 0       |
|       0x310 | IA32_LAPIC_REG_ICR1 | Interrupt Control Register, 63 .. 32      |
|-------------|---------------------|-------------------------------------------|

Local APIC configuration sequence
---------------------------------

1.  Mask all the interrupts in i8259
2.  Remap all of them properly so spurious interrupts don't generate exceptions
3.  Setup `LINTx` registers for LAPIC-originated (internal) interrupts
4.  Set the IRQ number spurious interrupts should be mapped to and soft-enable the
    LAPIC by writing `IA32_LAPIC_REG_SVR`
5.  Set the `IA32_LAPIC_REG_TPR` so that lower-priority interrupts don't get postponed
