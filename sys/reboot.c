#include "arch/amd64/acpica/acpi.h"
#include "arch/amd64/hw/io.h"
#include "user/reboot.h"
#include "sys/panic.h"
#include "sys/debug.h"

void system_power_cmd(unsigned int cmd) {
    switch (cmd) {
    case YGG_REBOOT_POWER_OFF:
        kinfo("ACPI poweroff...\n");
        AcpiEnterSleepStatePrep(5);
        asm volatile ("cli");
        AcpiEnterSleepState(5);
        panic("ACPI poweroff failed\n");
    case YGG_REBOOT_RESTART:
        kinfo("Rebooting...\n");
        // Use 8042:
        outb(0x64, 0xFE);
        __attribute__((fallthrough));
    case YGG_REBOOT_HALT:
        kinfo("Halting\n");
        // Guess just halting like this is fine
        while (1) {
            asm volatile ("cli; hlt");
        }
    default:
        panic("Unknown power command\n");
    }
}
