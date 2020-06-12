#include "sys/mem/vmalloc.h"
#include "arch/amd64/cpu.h"
#include "sys/mem/phys.h"
#include "sys/mem/slab.h"
#include "sys/snprintf.h"
#include "user/module.h"
#include "user/errno.h"
#include "sys/string.h"
#include "user/fcntl.h"
#include "sys/thread.h"
#include "sys/assert.h"
#include "sys/debug.h"
#include "sys/panic.h"
#include "fs/sysfs.h"
#include "sys/heap.h"
#include "fs/ofile.h"
#include "sys/elf.h"
#include "sys/mm.h"
#include "fs/vfs.h"

// Beyond 4GiB identity mapping
#define MOD_LINK_BASE               0x100000000
#define MOD_VIRT_BASE               (KERNEL_VIRT_BASE + 4 * (1UL << 30))
#define MOD_ENTRY_SYM               "_mod_entry"
#define MOD_EXIT_SYM                "_mod_exit"
#define MOD_DESC_SYM                "_mod_desc"

enum module_state {
    MOD_ACTIVE,
    MOD_ERROR
};

struct module {
    struct list_head list;
    enum module_state state;

    int (*entry_func) (void);
    void (*exit_func) (void);
    // TODO: maybe clone this? Only valid as long as the module
    //       is mapped
    struct module_desc *desc;

    uintptr_t base;
    size_t page_count;
};

static LIST_HEAD(module_list);
static struct slab_cache *module_cache = NULL;

static int elf_map_section(mm_space_t space, uintptr_t base, uintptr_t addr, size_t size) {
    // Convert size to pages, align addr down to page boundary
    size = (size + MM_PAGE_SIZE - 1) / MM_PAGE_SIZE;
    addr &= MM_PAGE_MASK;
    // Apply module offset
    addr += base;

    for (size_t i = 0; i < size; ++i) {
        uintptr_t virt = addr + i * MM_PAGE_SIZE;

        if (mm_map_get(space, virt, NULL) == MM_NADDR) {
            uintptr_t phys = mm_phys_alloc_page();
            _assert(phys != MM_NADDR);
            kdebug("%p: mapping to %p\n", virt, phys);
            _assert(mm_map_single(space, virt, phys, MM_PAGE_WRITE | MM_PAGE_GLOBAL, PU_KERNEL) == 0);
        } else {
            kdebug("%p: already mapped\n", virt);
        }
    }

    return 0;
}

static int elf_sym_lookup(const char *name, int64_t *result) {
    return debug_symbol_find_by_name(name, (uintptr_t *) result);
}

static int elf_sym_get(void *image, uintptr_t base, Elf64_Shdr *shdrs, size_t table, size_t index, int64_t *value) {
    if (table == SHN_UNDEF || index == SHN_UNDEF) {
        return -1;
    }

    Elf64_Shdr *shdr_symtab = &shdrs[table];
    size_t count = shdr_symtab->sh_size / sizeof(Elf64_Sym);

    if (index >= count) {
        kerror("Symbol index is outside of the table\n");
        return -1;
    }

    Elf64_Sym *sym = image + shdr_symtab->sh_offset + index * sizeof(Elf64_Sym);
    if (sym->st_shndx == SHN_UNDEF) {
        // Lookup external reference
        Elf64_Shdr *shdr_strtab = &shdrs[shdr_symtab->sh_link];
        const char *name = image + shdr_strtab->sh_offset + sym->st_name;

        if (elf_sym_lookup(name, value) != 0) {
            kerror("Undefined reference to %s\n", name);
            return -1;
        } else {
            return 0;
        }
    } else if (sym->st_shndx == SHN_ABS) {
        // TODO
        panic("1234\n");
    } else {
        // Internally defined symbol
        Elf64_Shdr *target = &shdrs[sym->st_shndx];
        *value = sym->st_value + target->sh_addr + base - MOD_LINK_BASE;
        return 0;
    }
}

#define R_X86_64_64             1
#define R_X86_64_PC32           2
#define R_X86_64_PLT32          4
#define R_X86_64_32             10

static int elf_rela_apply(void *image, uintptr_t base, Elf64_Shdr *shdrs, Elf64_Shdr *shdr_relatab) {
    Elf64_Rela *relatab = image + shdr_relatab->sh_offset;
    Elf64_Shdr *target = &shdrs[shdr_relatab->sh_info];
    Elf64_Ehdr *ehdr = image;

    (void) elf_sym_get;

    if (target->sh_flags & SHF_ALLOC) {
        for (size_t i = 0; i < shdr_relatab->sh_size / sizeof(Elf64_Rela); ++i) {
            Elf64_Rela *rela = &relatab[i];

            void *value_ptr = image + target->sh_offset + rela->r_offset;
            intptr_t value = 0;
            const char *target_name = image + shdrs[ehdr->e_shstrndx].sh_offset + target->sh_name;

            if (ELF64_R_SYM(rela->r_info) != SHN_UNDEF) {
                if (elf_sym_get(image,
                                base,
                                shdrs,
                                shdr_relatab->sh_link,
                                ELF64_R_SYM(rela->r_info),
                                &value) != 0) {
                    return -1;
                }
            }

            switch (ELF64_R_TYPE(rela->r_info)) {
            case R_X86_64_64:
                {
                    uint64_t src = (uint64_t) (value + (intptr_t) rela->r_addend);
                    // value_ptr most likely will be unaligned
                    memcpy(value_ptr, &src, sizeof(uint64_t));
                }
                break;
            case R_X86_64_PLT32:    // Treat this the same as PC32
            case R_X86_64_PC32:
                // Symbol + Offset - Section Offset
                {
                    intptr_t off = value +
                                   (intptr_t) rela->r_offset -
                                   (base + target->sh_addr - MOD_LINK_BASE);
                    if (!!(off & 0x8000000000000000) != !!(off & 0x80000000)) {
                        kfatal("From: %p <%04x + %s>\n",
                            rela->r_offset + base + target->sh_addr - MOD_LINK_BASE,
                            rela->r_offset,
                            target_name);
                        kfatal("Against: %p\n", value);
                        panic("Cannot fit relocation: %ld (%p)\n", off, off);
                    }
                    *(int32_t *) value_ptr = (int32_t) off;
                }
                break;
            default:
                panic("Unsupported relocation type: %02x\n", ELF64_R_TYPE(rela->r_info));
            }
        }
    }

    return 0;
}

static int elf_mod_load(struct module *mod, struct vfs_ioctx *ctx, struct ofile *fd) {
    Elf64_Ehdr *ehdr;
    Elf64_Shdr *shdrs;
    Elf64_Shdr *shdr_symtab = NULL,
               *shdr_strtab = NULL,
               *shdr_shstrtab = NULL;
    char *strtab, *shstrtab;
    Elf64_Sym *symtab;
    uintptr_t base;
    size_t virt_size = 0;
    struct stat st;
    void *image;
    int res = 0;

    // Load the whole module image into memory
    if ((res = vfs_fstat(ctx, fd, &st)) != 0) {
        return res;
    }

    kdebug("Image size: %S\n", st.st_size);

    image = kmalloc(st.st_size);
    if (!image) {
        kerror("Failed to allocate memory for module image\n");
        return -ENOMEM;
    }

    size_t total = 0;
    size_t rem = st.st_size;
    ssize_t bread;

    // Chunked read (1024 bytes per read)
    while ((bread = vfs_read(ctx, fd, image + total, MIN(rem, 1024))) > 0) {
        total += bread;
        rem -= bread;
    }

    if (rem != 0) {
        kerror("Failed to load module image\n");
        res = -EINVAL;
        goto end;
    }

    ehdr = image;
    // TODO: check magic and whatever
    shdrs = ehdr->e_shoff + image;
    shdr_shstrtab = &shdrs[ehdr->e_shstrndx];
    shstrtab = image + shdr_shstrtab->sh_offset;

    // Find relevant sections
    for (size_t i = 0; i < ehdr->e_shnum; ++i) {
        Elf64_Shdr *shdr = &shdrs[i];
        const char *name = &shstrtab[shdr->sh_name];

        if (shdr->sh_type == SHT_STRTAB && !strcmp(name, ".strtab")) {
            _assert(!shdr_strtab);
            shdr_strtab = shdr;
        }
        if (shdr->sh_type == SHT_SYMTAB) {
            _assert(!shdr_symtab);
            shdr_symtab = shdr;
        }
    }

    _assert(shdr_symtab);
    _assert(shdr_strtab);

    strtab = shdr_strtab->sh_offset + image;
    symtab = shdr_symtab->sh_offset + image;

    // Find out virtual memory length for module
    for (size_t i = 0; i < ehdr->e_shnum; ++i) {
        Elf64_Shdr *shdr = &shdrs[i];
        if (shdr->sh_flags & SHF_ALLOC) {
            if (shdr->sh_addr >= MOD_LINK_BASE && shdr->sh_size + shdr->sh_addr - MOD_LINK_BASE > virt_size) {
                virt_size = shdr->sh_size + shdr->sh_addr - MOD_LINK_BASE;
            }
        }
    }

    // Convert to page count
    virt_size = (virt_size + MM_PAGE_SIZE - 1) / MM_PAGE_SIZE;

    // Allocate a virtual memory region for the module
    base = vmfind(mm_kernel, MOD_VIRT_BASE, MOD_VIRT_BASE + 4 * (1UL << 30), virt_size);
    if (base == MM_NADDR) {
        panic("Failed to allocate memory for module\n");
    }
    // Module is now based
    mod->base = base;
    mod->page_count = virt_size;

    // Perform symbol relocations
    for (size_t i = 0; i < ehdr->e_shnum; ++i) {
        Elf64_Shdr *shdr = &shdrs[i];

        if (shdr->sh_type == SHT_RELA) {
            if (elf_rela_apply(image, base, shdrs, shdr) != 0) {
                res = -EINVAL;
                goto end;
            }
        }
    }

    // Load BITS sections
    for (size_t i = 0; i < ehdr->e_shnum; ++i) {
        Elf64_Shdr *shdr = &shdrs[i];

        if (shdr->sh_flags & SHF_ALLOC) {
            const char *name = &shstrtab[shdr->sh_name];
            _assert(shdr->sh_addr >= MOD_LINK_BASE);
            _assert(elf_map_section(mm_kernel, base, shdr->sh_addr - MOD_LINK_BASE, shdr->sh_size) == 0);

            switch (shdr->sh_type) {
            case SHT_PROGBITS:
                memcpy((void *) (shdr->sh_addr + base - MOD_LINK_BASE), image + shdr->sh_offset, shdr->sh_size);
                break;
            case SHT_NOBITS:
                memset((void *) (shdr->sh_addr + base - MOD_LINK_BASE), 0, shdr->sh_size);
                break;
            }
        }
    }

    // Find relevant symbols
    for (size_t i = 0; i < shdr_symtab->sh_size / sizeof(Elf64_Sym); ++i) {
        Elf64_Sym *sym = &symtab[i];
        if (sym->st_name > shdr_strtab->sh_size) {
            continue;
        }
        const char *name = &strtab[sym->st_name];
        Elf64_Shdr *shdr = &shdrs[sym->st_shndx];
        uintptr_t addr = sym->st_value + base - MOD_LINK_BASE + shdr->sh_addr;

        if (!strcmp(name, MOD_ENTRY_SYM)) {
            if (ELF_ST_TYPE(sym->st_info) != STT_FUNC) {
                panic("%s: not a function symbol\n", name);
            }
            mod->entry_func = (void *) addr;
        } else if (!strcmp(name, MOD_EXIT_SYM)) {
            if (ELF_ST_TYPE(sym->st_info) != STT_FUNC) {
                panic("%s: not a function symbol\n", name);
            }
            mod->exit_func = (void *) addr;
        } else if (!strcmp(name, MOD_DESC_SYM)) {
            mod->desc = (void *) addr;
        }
    }
end:
    kfree(image);

    return res;
}

static int module_enter(struct module *mod) {
    return mod->entry_func();
}

static void module_destroy(struct module *mod) {
    if (mod->base) {
        for (size_t i = 0; i < mod->page_count; ++i) {
            uintptr_t mapping = mm_map_get(mm_kernel, mod->base + i * MM_PAGE_SIZE, NULL);

            if (mapping != MM_NADDR) {
                kdebug("Releasing module page: %p\n", mapping);

                _assert(mm_umap_single(mm_kernel, mod->base + i * MM_PAGE_SIZE, 1) == mapping);
            }
        }
    }
    slab_free(module_cache, mod);
}

int sys_module_unload(const char *name) {
    struct thread *thr = thread_self;

    if (thr->ioctx.uid != 0) {
        return -EACCES;
    }

    struct module *mod = NULL;
    struct module *it;
    list_for_each_entry(it, &module_list, list) {
        if (!strcmp(it->desc->name, name)) {
            mod = it;
            break;
        }
    }

    if (!mod) {
        return -ENOENT;
    }

    kinfo("Unloading %s\n", mod->desc->name);

    if (mod->exit_func) {
        mod->exit_func();
    }

    list_del(&mod->list);
    module_destroy(mod);

    return 0;
}

int sys_module_load(const char *path, const char *params) {
    struct vfs_ioctx kernel_ioctx = {0, 0, 0};
    int res;
    struct ofile fd;
    struct thread *thr = thread_self;

    if (thr->ioctx.uid != 0) {
        return -EACCES;
    }

    // Create a new module
    if (!module_cache) {
        _assert(module_cache = slab_cache_get(sizeof(struct module)));
    }
    struct module *mod = slab_calloc(module_cache);
    if (!mod) {
        kerror("Failed to allocate module struct\n");
        return -ENOMEM;
    }
    list_head_init(&mod->list);

    userptr_check(path);
    if (params) {
        userptr_check(params);
    } else {
        params = "";
    }

    kinfo("Load module %s\n", path);

    if ((res = vfs_open(&kernel_ioctx, &fd, path, O_RDONLY, 0)) != 0) {
        kerror("%s: %s\n", path, kstrerror(res));
        slab_free(module_cache, mod);
        return res;
    }

    if ((res = elf_mod_load(mod, &kernel_ioctx, &fd)) != 0) {
        vfs_close(&kernel_ioctx, &fd);
        kerror("elf load failed: %s\n", kstrerror(res));
        slab_free(module_cache, mod);
        return res;
    }

    vfs_close(&kernel_ioctx, &fd);

    if (!mod->desc) {
        kerror("%s: module doesn't have description structure\n", path);
        module_destroy(mod);
        return -EINVAL;
    }

    if (!mod->entry_func) {
        kerror("%s: module doesn't have an entry function\n", path);
        module_destroy(mod);
        return -EINVAL;
    }

    if ((res = module_enter(mod)) != 0) {
        module_destroy(mod);
        return res;
    }

    list_add(&mod->list, &module_list);

    kdebug("Loaded %s\n", mod->desc->name);

    return 0;
}

int mod_list(void *ctx, char *buf, size_t lim) {
    struct module *mod;
    list_for_each_entry(mod, &module_list, list) {
        sysfs_buf_printf(buf, lim, "%s %s %p %u\n",
                mod->desc->name,
                mod->state == MOD_ACTIVE ? "active" : "error",
                mod->base,
                mod->page_count);
    }
    return 0;
}
