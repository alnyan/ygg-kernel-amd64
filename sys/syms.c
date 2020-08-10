#include "sys/syms.h"
#include "sys/mm.h"
#include "sys/string.h"
#include "sys/panic.h"
#include "sys/debug.h"
#include "sys/assert.h"
#include "sys/hash.h"
#include "arch/amd64/multiboot2.h"

////

static uintptr_t g_symtab_ptr = 0;
static uintptr_t g_strtab_ptr = 0;
static size_t g_symtab_size = 0;
static size_t g_strtab_size = 0;
static struct hash g_symtab_hash;

// Multiboot2-loaded sections are misaligned for some reason
void ksym_set_multiboot2(struct multiboot_tag_elf_sections *tag) {
    kinfo("Loading kernel symbols\n");
    kinfo("%u section headers:\n", tag->num);
    size_t string_section_offset = tag->shndx * tag->entsize;
    Elf64_Shdr *string_section_hdr = (Elf64_Shdr *) &tag->sections[string_section_offset];
    const char *shstrtab = (const char *) MM_VIRTUALIZE(realigned(string_section_hdr, sh_addr));

    Elf64_Shdr *symtab_shdr = NULL, *strtab_shdr = NULL;

    for (size_t i = 0; i < tag->num; ++i) {
        Elf64_Shdr *shdr = (Elf64_Shdr *) &tag->sections[i * tag->entsize];
        const char *name = &shstrtab[realigned(shdr, sh_name)];

        switch (realigned(shdr, sh_type)) {
        case SHT_SYMTAB:
            if (symtab_shdr) {
                panic("Kernel image has 2+ symbol tables\n");
            }
            symtab_shdr = shdr;
            break;
        case SHT_STRTAB:
            if (strcmp(name, ".strtab")) {
                break;
            }
            if (strtab_shdr) {
                panic("kernel image has 2+ string tables\n");
            }
            strtab_shdr = shdr;
            break;
        default:
            break;
        }
    }

    if (symtab_shdr && strtab_shdr) {
        uintptr_t symtab_ptr = MM_VIRTUALIZE(realigned(symtab_shdr, sh_addr));
        uintptr_t strtab_ptr = MM_VIRTUALIZE(realigned(strtab_shdr, sh_addr));

        ksym_set_tables(symtab_ptr,
                        strtab_ptr,
                        realigned(symtab_shdr, sh_size),
                        realigned(strtab_shdr, sh_size));
    }
}

void ksym_set_tables(uintptr_t s0, uintptr_t s1, size_t z0, size_t z1) {
    Elf64_Sym *sym;
    Elf64_Word type, bind;
    const char *name;

    _assert(shash_init(&g_symtab_hash, 32) == 0);

    // TODO: entsize?
    for (size_t i = 0; i < z0 / sizeof(Elf64_Sym); ++i) {
        sym = (Elf64_Sym *) (s0 + i * sizeof(Elf64_Sym));
        type = ELF64_ST_TYPE(sym->st_info);
        bind = ELF64_ST_BIND(sym->st_info);
        if (type != STT_SECTION && bind == STB_GLOBAL) {
            name = (const char *) s1 + sym->st_name;
            _assert(hash_insert(&g_symtab_hash, name, sym) == 0);
        }
    }

    // Fallback
    g_symtab_ptr = s0;
    g_symtab_size = z0;
    g_strtab_ptr = s1;
    g_strtab_size = z1;
}

static Elf64_Sym *ksym_lookup_fast(const char *name) {
    struct hash_pair *p = hash_lookup(&g_symtab_hash, name);
    if (p) {
        return p->value;
    } else {
        return NULL;
    }
}

Elf64_Sym *ksym_lookup(const char *name) {
    Elf64_Sym *res = ksym_lookup_fast(name);

    if (res) {
        return res;
    }

    if (!g_symtab_ptr) {
        kwarn("no symbol table\n");
        return NULL;
    }

    Elf64_Sym *symtab = (Elf64_Sym *) g_symtab_ptr;
    for (size_t i = 0; i < g_symtab_size / sizeof(Elf64_Sym); ++i) {
        Elf64_Sym *sym = &symtab[i];
        if (sym->st_name < g_strtab_size) {
            const char *r_name = (const char *) g_strtab_ptr + sym->st_name;
            if (!strcmp(r_name, name)) {
                return sym;
            }
        }
    }

    return NULL;
}

int ksym_find_location(uintptr_t addr, const char **name, uintptr_t *base) {
    if (!g_symtab_ptr) {
        kwarn("no symbol table\n");
        return -1;
    }

    if (g_symtab_ptr && g_strtab_ptr) {
        size_t offset = 0;
        Elf64_Sym *sym;

        while (offset < g_symtab_size) {
            sym = (Elf64_Sym *) (g_symtab_ptr + offset);

            if (ELF_ST_TYPE(sym->st_info) == STT_FUNC) {
                if (sym->st_value <= addr && sym->st_value + sym->st_size >= addr) {
                    *base = sym->st_value;
                    if (sym->st_name < g_strtab_size) {
                        *name = (const char *) (g_strtab_ptr + sym->st_name);
                    } else {
                        *name = "<invalid>";
                    }
                    return 0;
                }
            }

            offset += sizeof(Elf64_Sym);
        }
    }

    return -1;
}

