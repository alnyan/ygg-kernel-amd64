#include "arch/amd64/mm/phys.h"
#include "sys/debug.h"

void amd64_phys_memory_map(const multiboot_memory_map_t *mmap, size_t length) {
    static const char *mmap_region_strings[6] = {
        "Unknown", "Available", "Used", "ACPI", "Reserved", "Defective"
    };
    kdebug("Memory map @ %p\n", mmap);
    uintptr_t curr_item = (uintptr_t) mmap;
    uintptr_t mmap_end = length + curr_item;

    while (curr_item < mmap_end) {
        const multiboot_memory_map_t *entry = (const multiboot_memory_map_t *) (curr_item);
        size_t type_index = (entry->type >= (sizeof(mmap_region_strings) / sizeof(mmap_region_strings[0]))) ? 0 : entry->type;

        kdebug("%p - %p: %s\n", entry->addr, entry->addr + entry->size, mmap_region_strings[type_index]);

        curr_item += entry->size + sizeof(uint32_t);
    }
}
