#include "arch/amd64/smp/smp.h"
#include "arch/amd64/hw/apic.h"
#include "arch/amd64/hw/timer.h"
#include "arch/amd64/hw/gdt.h"
#include "arch/amd64/hw/idt.h"
#include "arch/amd64/mm/mm.h"
#include "arch/amd64/syscall.h"
#include "arch/amd64/cpu.h"
#include "arch/amd64/fpu.h"
#include "sys/string.h"
#include "sys/panic.h"
#include "sys/sched.h"
#include "sys/debug.h"

#define SMP_AP_BOOTSTRAP_CODE       0x7000
#define SMP_AP_BOOTSTRAP_DATA       0x7FC0

#define CPU_READY                   (1 << 0)

struct ap_param_block {
    uint64_t cr3;
    uint64_t gdtr_phys;
    uint64_t idtr;
    uint64_t entry;
    uint64_t rsp;
    uint64_t cpu_no;
};

extern char kernel_stacks_top[];
// TODO: use mutual exclusion for this
size_t smp_ncpus = 1;
static inline void set_cpu(uintptr_t base) {
    // Write kernelGSbase again
    wrmsr(MSR_IA32_KERNEL_GS_BASE, base);
    asm volatile ("swapgs");
    // Write kernelGSbase again
    wrmsr(MSR_IA32_KERNEL_GS_BASE, base);
}

static void amd64_ap_code_entry(void) {
    {
        struct ap_param_block *appb = (struct ap_param_block *) SMP_AP_BOOTSTRAP_DATA;
        uint8_t cpu_no = (uint8_t) appb->cpu_no;
        // Setup %gs for this CPU
        set_cpu((uintptr_t) &cpus[cpu_no]);

        // After this, 0 mapping may be removed by BSP, so
        // using appb is impossible past this scope
    }
    struct cpu *cpu = get_cpu();

    // This is atomic
    cpu->flags = CPU_READY;

    // Setup IDT for this AP
    amd64_idt_init(cpu->processor_id);

    // Enable LAPIC.SVR.SoftwareEnable bit
    // And set spurious interrupt mapping to 0xFF
    LAPIC(LAPIC_REG_SVR) |= (1 << 8) | (0xFF);

    kdebug("cpu%d is online\n", cpu->processor_id);

    // Enable FPU
    amd64_fpu_init();

    // Enable LAPIC timer
    amd64_timer_init();

    syscall_init();

    do {
        asm volatile ("sti; hlt; cli");
    } while (!sched_ready);

    sched_enter();

    panic("cpu%u failed to start scheduler\n", cpu->processor_id);
}

void amd64_load_ap_code(void) {
    extern const char amd64_ap_code_start[];
    extern const char amd64_ap_code_end[];
    //extern void amd64_ap_code_entry(void);
    size_t ap_code_size = (uintptr_t) amd64_ap_code_end - (uintptr_t) amd64_ap_code_start;

    // Load code at 0x7000
    memcpy((void *) SMP_AP_BOOTSTRAP_CODE, amd64_ap_code_start, ap_code_size);

    // These parameters are shared and may be loaded only once
    // Write AP code startup parameters
    struct ap_param_block *appb = (struct ap_param_block *) SMP_AP_BOOTSTRAP_DATA;
    extern mm_space_t mm_kernel;
    uintptr_t mm_kernel_phys = (uintptr_t) mm_kernel - 0xFFFFFF0000000000;

    // 0x7FC0 - MM_PHYS(mm_kernel)
    appb->cr3 = mm_kernel_phys;
    // 0x7FD8 - amd64_core_entry
    appb->entry = (uintptr_t) amd64_ap_code_entry;
}

static void amd64_set_ap_params(uint8_t cpu_no) {
    // Allocate a new AP kernel stack
    uintptr_t stack_ptr = (uintptr_t) kernel_stacks_top - (cpu_no - 1) * 65536;
    uintptr_t amd64_gdtr_phys = (uintptr_t) amd64_gdtr_get(cpu_no) - 0xFFFFFF0000000000;
    struct ap_param_block *appb = (struct ap_param_block *) SMP_AP_BOOTSTRAP_DATA;
    // 0x7FC8 - MM_PHYS(amd64_gdtr)
    appb->gdtr_phys = amd64_gdtr_phys;
    // 0x7FE0 - stack_ptr
    appb->rsp = stack_ptr;
    // 0x7FE8
    appb->cpu_no = cpu_no;
}

void amd64_smp_ap_initialize(uint8_t cpu_no) {
    kdebug("Starting up cpu%d\n", cpu_no);
    uint8_t apic_id = cpus[cpu_no].apic_id;

    amd64_set_ap_params(cpu_no);

    uint8_t entry_vector = SMP_AP_BOOTSTRAP_CODE >> 12;

    LAPIC(LAPIC_REG_CMD1) = ((uint32_t) apic_id) << 24;
    LAPIC(LAPIC_REG_CMD0) = entry_vector | (5 << 8) | (1 << 14);

    for (uint64_t i = 0; i < 1000000; ++i) {
        asm volatile ("pause");
    }

    LAPIC(LAPIC_REG_CMD1) = ((uint32_t) apic_id) << 24;
    LAPIC(LAPIC_REG_CMD0) = entry_vector | (6 << 8) | (1 << 14);

    for (uint64_t i = 0; i < 10000000; ++i) {
        asm volatile ("pause");
    }

    if (cpus[cpu_no].flags != CPU_READY) {
        kdebug("Failed to start cpu%d\n", cpu_no);
        while (1) {
            asm ("cli; hlt");
        };
    }
}

void amd64_smp_add(uint8_t apic_id) {
    if (smp_ncpus == AMD64_MAX_SMP) {
        // TODO: panic();
        kfatal("Kernel does not support ncpus >%u\n", smp_ncpus);
        while (1);
    }

    size_t i = smp_ncpus++;
    cpus[i].flags = 0;
    cpus[i].self = &cpus[i];
    cpus[i].apic_id = (uint64_t) apic_id;
    cpus[i].processor_id = i;
    cpus[i].tss = amd64_tss_get(i);
}

void amd64_smp_bsp_configure(void) {
    // Set %gs for BSP
    cpus[0].flags = CPU_READY;
    cpus[0].self = &cpus[0];
    cpus[0].processor_id = 0;
    cpus[0].apic_id = 0 /* TODO: apic_id may be different from 0 for BSP */;
    cpus[0].tss = amd64_tss_get(0);
    cpus[0].thread = NULL;
    set_cpu((uintptr_t) &cpus[0]);
}

void amd64_smp_init(void) {
    kdebug("SMP init\n");

    for (size_t i = 1; i < smp_ncpus; ++i) {
        amd64_smp_ap_initialize(i);
    }

    sched_set_ncpus(smp_ncpus);
}

