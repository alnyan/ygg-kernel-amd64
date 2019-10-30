#include "sys/binfmt_elf.h"
#include "sys/debug.h"
#include "sys/string.h"
#include "sys/panic.h"
#include "sys/amd64/mm/phys.h"
#include "sys/amd64/mm/map.h"
#include "sys/assert.h"
#include "sys/errno.h"
#include "sys/elf.h"

#define ELF_ADDR_MIN 0x400000

// uintptr_t amd64_map_get(const mm_space_t space, uintptr_t vaddr, uint64_t *flags);
// int amd64_map_single(mm_space_t pml4, uintptr_t virt_addr, uintptr_t phys, uint32_t flags);

static int elf_map_section(mm_space_t space, uintptr_t vma_dst, size_t size) {
    uintptr_t page_aligned = vma_dst & ~0xFFF;
    uintptr_t page_offset = vma_dst & 0xFFF;
    size_t npages = (size + page_offset + 4095) / 4096;
    uintptr_t page_phys;

    kdebug("Section needs %d pages\n", npages);

    for (size_t i = 0; i < npages; ++i) {
        if ((page_phys = amd64_map_get(space, page_aligned + (i << 12), NULL)) == MM_NADDR) {
            // Allocation needed
            assert((page_phys = amd64_phys_alloc_page()) != MM_NADDR,
                    "Failed to allocate memory\n");
            assert(amd64_map_single(space, page_aligned + (i << 12), page_phys, (1 << 1) | (1 << 2)) == 0,
                    "Failed to map memory\n");
        } else {
            kdebug("%p = %p (already)\n", page_aligned + (i << 12), page_phys);
        }
    }

    return 0;
}

static int elf_memcpy(mm_space_t space, uintptr_t vma_dst, uintptr_t load_src, size_t size) {
    uintptr_t page_aligned = vma_dst & ~0xFFF;
    uintptr_t page_offset = vma_dst & 0xFFF;
    size_t npages = (size + page_offset + 4095) / 4096;
    uintptr_t page_phys;
    size_t rem = size;
    size_t off = 0;

    kdebug("memcpy %p <- %p, %S\n", vma_dst, load_src, size);
    for (size_t i = 0; i < npages; ++i) {
        assert((page_phys = amd64_map_get(space, page_aligned + (i << 12), NULL)), "What?");
        size_t nbytes = MIN(rem, 4096 - page_offset);
        kdebug("Copy %u bytes in page %p + %04x (%u) <- %p\n", nbytes, page_phys, page_offset, i, load_src + off);
        memcpy((void *) MM_VIRTUALIZE(page_phys + page_offset), (void *) (load_src + off), nbytes);
        rem -= nbytes;
        off += nbytes;
        page_offset = 0;
    }

    return 0;
}

static int elf_bzero(mm_space_t space, uintptr_t vma_dst, size_t size) {
    uintptr_t page_aligned = vma_dst & ~0xFFF;
    uintptr_t page_offset = vma_dst & 0xFFF;
    size_t npages = (size + page_offset + 4095) / 4096;
    uintptr_t page_phys;
    size_t rem = size;

    kdebug("bzero %p, %S\n", vma_dst, size);
    for (size_t i = 0; i < npages; ++i) {
        assert((page_phys = amd64_map_get(space, page_aligned + (i << 12), NULL)), "What?");
        size_t nbytes = MIN(rem, 4096 - page_offset);
        kdebug("Zero %u bytes in page %p + %04x (%u)\n", nbytes, page_phys, page_offset, i);
        memset((void *) MM_VIRTUALIZE(page_phys + page_offset), 0, nbytes);
        rem -= nbytes;
        page_offset = 0;
    }

    return 0;
}

int elf_load(struct thread *thr, const void *from) {
    struct amd64_thread *thr_plat = &thr->data;
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

    thr->image.image_end = 0;

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

            // Map the pages of this section
            _assert(elf_map_section(thr->space, shdr->sh_addr, shdr->sh_size) == 0);

            switch (shdr->sh_type) {
            case SHT_PROGBITS:
                _assert(elf_memcpy(thr->space,
                                   shdr->sh_addr,
                                   shdr->sh_offset + (uintptr_t) from,
                                   shdr->sh_size) == 0);
                break;
            case SHT_NOBITS:
                _assert(elf_bzero(thr->space,
                                  shdr->sh_addr,
                                  shdr->sh_size) == 0);
                break;
            }

            uintptr_t section_end = shdr->sh_addr + shdr->sh_size;
            if (section_end > thr->image.image_end) {
                thr->image.image_end = section_end;
            }
        }
    }

    thr->image.brk = thr->image.image_end;
    amd64_thread_set_ip(thr, ehdr->e_entry);

    return 0;
}
