#pragma once
#include "sys/types.h"
#include "sys/elf.h"

struct elf_sections {
    enum {
        KSYM_TABLE_NONE,
        KSYM_TABLE_MULTIBOOT2,
    } kind;
    union {
        struct multiboot_tag_elf_sections *multiboot2;
    } tables;
};

void ksym_set_tables(uintptr_t symtab, uintptr_t strtab, size_t symtab_size, size_t strtab_size);
void ksym_set(struct elf_sections *sections);
Elf64_Sym *ksym_lookup(const char *name);
int ksym_find_location(uintptr_t addr, const char **name, uintptr_t *base);

int ksym_load(void);
