#pragma once
#include "sys/types.h"
#include "sys/elf.h"

struct multiboot_tag_elf_sections;

void ksym_set_tables(uintptr_t symtab, uintptr_t strtab, size_t symtab_size, size_t strtab_size);
void ksym_set_multiboot2(struct multiboot_tag_elf_sections *tag);
Elf64_Sym *ksym_lookup(const char *name);
int ksym_find_location(uintptr_t addr, const char **name, uintptr_t *base);

int ksym_load(void);
