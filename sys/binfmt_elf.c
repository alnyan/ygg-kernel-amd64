#include "sys/binfmt_elf.h"
#include "sys/debug.h"
#include "sys/mem.h"
#include "sys/string.h"
#include "sys/panic.h"
#include "sys/amd64/mm/phys.h"
#include "sys/amd64/mm/map.h"
#include "sys/errno.h"
#include "sys/elf.h"

#define ELF_ADDR_MIN 0x400000

int elf_load(thread_t *thr, const void *from) {
    struct thread_info *thri = thread_get(thr);

    Elf64_Ehdr *ehdr = (Elf64_Ehdr *) from;
    // Check magic
    if (strncmp((const char *) ehdr->e_ident, "\x7F""ELF", 4) != 0) {
        kerror("elf: magic mismatch\n");
        return -EINVAL;
    }

    if (ehdr->e_ident[EI_CLASS] != ELFCLASS64) {
        kerror("elf: object was not intended for 64-bit\n");
        return -EINVAL;
    }

    Elf64_Shdr *shdrs = (Elf64_Shdr *) (ehdr->e_shoff + (uintptr_t) from);
    const char *shstrtabd = (const char *) (shdrs[ehdr->e_shstrndx].sh_offset + (uintptr_t) from);

    // Load the sections
    for (size_t i = 0; i < ehdr->e_shnum; ++i) {
        Elf64_Shdr *shdr = &shdrs[i];
        const char *name = &shstrtabd[shdr->sh_name];

        if (shdr->sh_flags & SHF_ALLOC) {
            if (!strncmp(name, ".note", 5)) {
                // Fuck you, gcc
                continue;
            }
            if (!strncmp(name, ".gnu", 4)) {
                // Fuck you, gcc
                continue;
            }

            kdebug("Loading %s\n", name);

            // If the section is below what is allowed
            if (shdr->sh_addr < ELF_ADDR_MIN) {
                kerror("elf: section address is below allowed\n");
                return -EINVAL;
            }

            // Allocate memory for the section
            size_t sec_pages = (shdr->sh_size + 0xFFF) / 0x1000;
            kdebug("%s needs %u pages\n", name, sec_pages);
            if (sec_pages > 1) {
                panic("elf: I was too lazy to implement this yet\n");
            }

            uintptr_t page_virt = shdr->sh_addr & ~0xFFF;
            uintptr_t page_offset = shdr->sh_addr & 0xFFF;

            kdebug("%s base page is VMA %p\n", name, page_virt);

            uintptr_t page_phys;

            if ((page_phys = amd64_map_get(thri->space, page_virt, NULL)) == MM_NADDR) {
                page_phys = amd64_phys_alloc_page();

                if (page_phys == MM_NADDR) {
                    panic("elf: out of memory\n");
                }

                if (amd64_map_single(thri->space, page_virt, page_phys, (1 << 1) | (1 << 2)) != 0) {
                    panic("elf: map failed\n");
                }
            } else {
                kdebug("Not mapping\n");
            }

            if (shdr->sh_type == SHT_PROGBITS) {
                kdebug("elf: memcpy %p <- %p %S\n", MM_VIRTUALIZE(page_phys) + page_offset,
                                               (uintptr_t) from + shdr->sh_offset,
                                               shdr->sh_size);
                memcpy((void *) (MM_VIRTUALIZE(page_phys) + page_offset),
                       (const void *) ((uintptr_t) from + shdr->sh_offset),
                       shdr->sh_size);
            } else if (shdr->sh_type == SHT_NOBITS) {
                kdebug("elf: memset 0 %p %S\n", MM_VIRTUALIZE(page_phys) + page_offset,
                                           shdr->sh_size);
                memset((void *) (MM_VIRTUALIZE(page_phys) + page_offset),
                       0, shdr->sh_size);
            }
        }
    }

    thread_set_ip(thr, ehdr->e_entry);

    return 0;
}
