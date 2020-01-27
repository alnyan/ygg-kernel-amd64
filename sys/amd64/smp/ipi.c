#include "sys/amd64/hw/apic.h"
#include "sys/amd64/smp/smp.h"
#include "sys/amd64/smp/ipi.h"
#include "sys/amd64/cpu.h"
#include "sys/debug.h"

void amd64_ipi_send(int cpu, uint8_t vector) {
    if (cpu < 0 || cpu >= (int) smp_ncpus) {
        kerror("Invalid cpu number: %d\n", cpu);
    }

    LAPIC(LAPIC_REG_CMD1) = ((uint32_t) (cpus[cpu].apic_id & 0xFF)) << 24;
    // Wait for delivery status bit to clear
    while (LAPIC(LAPIC_REG_CMD0) & (1 << 12));
    // Command: vector 0xF0,
    LAPIC(LAPIC_REG_CMD0) = vector | (1 << 14);
}

void amd64_ipi_handle(void) {
    kdebug("IPI received by cpu%u\n", get_cpu()->processor_id);
}

void amd64_ipi_panic(void) {
    kfatal("cpu%u received panic IPI: halting\n", get_cpu()->processor_id);

    while (1) {
        asm volatile ("cli; hlt");
    }
}
