#include "arch/amd64/cpu.h"
#include "fs/ofile.h"
#include "fs/sysfs.h"
#include "fs/vfs.h"
#include "sys/assert.h"
#include "sys/debug.h"
#include "sys/elf.h"
#include "sys/mem/phys.h"
#include "sys/mem/slab.h"
#include "sys/mem/vmalloc.h"
#include "sys/heap.h"
#include "sys/string.h"
#include "sys/thread.h"
#include "sys/panic.h"
#include "user/errno.h"
#include "user/fcntl.h"

#define MODULE_MAP_START        0x100000000
#define MODULE_MAP_END          0xF00000000
#define SYM_MOD_DESC            "_mod_desc"
#define SYM_MOD_ENTER           "_mod_enter"
#define SYM_MOD_EXIT            "_mod_exit"

struct object_image {
    void        *base;
    size_t       size;

    const char  *shstrtab;
    const char  *symstrtab;
    Elf64_Sym   *symtab;
    Elf64_Shdr  *sh_symtab;
};

struct object {
    struct module_desc *module_desc;
    int (*module_enter) (void *);
    void (*module_exit) (void);

    struct object_image image;

    // In-memory object
    uintptr_t object_base;
    size_t object_page_count;

    // PLT and GOT
    void *gotplt;
    size_t gotplt_size;
};

static struct slab_cache *g_object_cache;

static struct object *object_create(void) {
    if (!g_object_cache) {
        g_object_cache = slab_cache_get(sizeof(struct object));
    }
    return slab_calloc(g_object_cache);
}

static void object_free(struct object *obj) {
    kfree(obj->image.base);
    // TODO: free pages occupied by the object
    slab_free(g_object_cache, obj);
}

static void object_finalize_load(struct object *obj) {
    // Image no longer needed
    kfree(obj->image.base);
    memset(&obj->image, 0, sizeof(struct object_image));
}

static int object_read(struct object *obj, struct vfs_ioctx *ctx, const char *pathname) {
    struct ofile of = {0};
    struct stat st;
    int res;

    if ((res = vfs_openat(ctx, &of, NULL, pathname, O_RDONLY, 0)) != 0) {
        return res;
    }
    if ((res = vfs_fstatat(ctx, of.file.vnode, NULL, &st, AT_EMPTY_PATH)) != 0) {
        goto cleanup;
    }

    obj->image.size = st.st_size;
    // TODO: optimize how loading is handled?
    obj->image.base = kmalloc(obj->image.size);
    if (!obj->image.base) {
        res = -ENOMEM;
        goto cleanup;
    }

    if ((res = vfs_read(ctx, &of, obj->image.base, obj->image.size)) != (int) obj->image.size) {
        if (res >= 0) {
            kwarn("File read returned %S, expected to read %S\n", res, obj->image.size);
            res = -EINVAL;
        }
        goto cleanup;
    }

    res = 0;
cleanup:
    vfs_close(ctx, &of);
    return res;
}

static int object_section_load(struct object *obj, Elf64_Shdr *shdr) {
    uintptr_t page_phys;
    uintptr_t page_start = (shdr->sh_addr + obj->object_base) & ~0xFFF;
    uintptr_t page_end = (shdr->sh_addr + obj->object_base + shdr->sh_size + 0xFFF) & ~0xFFF;

    for (uintptr_t page = page_start; page < page_end; page += MM_PAGE_SIZE) {
        // Get or map physical page
        if (mm_map_get(mm_kernel, page, &page_phys) == MM_NADDR) {
            kdebug("MAP OBJECT PAGE %p\n", page);
            page_phys = mm_phys_alloc_page();
            _assert(page_phys != MM_NADDR);
            _assert(mm_map_single(mm_kernel, page, page_phys, MM_PAGE_WRITE, 0) == 0);
        }
    }

    switch (shdr->sh_type) {
    case SHT_NOBITS:
        memset((void *) (shdr->sh_addr + obj->object_base), 0, shdr->sh_size);
        break;
    case SHT_PROGBITS:
        kdebug("Load: %p <- %p, %S\n", shdr->sh_addr, shdr->sh_offset, shdr->sh_size);
        memcpy((void *) (shdr->sh_addr + obj->object_base), obj->image.base + shdr->sh_offset, shdr->sh_size);
        break;
    default:
        panic("Unhandled section type: %02x\n", shdr->sh_type);
    }

    return 0;
}

static inline Elf64_Shdr *object_shdr(struct object *obj, size_t index) {
    return obj->image.base + ((Elf64_Ehdr *) obj->image.base)->e_shoff +
           ((Elf64_Ehdr *) obj->image.base)->e_shentsize * index;
}

static int object_reloc(struct object *obj) {
    Elf64_Ehdr *ehdr;
    Elf64_Shdr *shdr;
    Elf64_Rela *relatab;
    const char *name;

    ehdr = obj->image.base;

    // Map pages for GOT/PLT
    size_t pltoff = 0;
    uint8_t *plt = obj->gotplt;
    _assert(!(((uintptr_t) plt) & 0xFFF));
    for (size_t i = 0; i < obj->gotplt_size; i += MM_PAGE_SIZE) {
        uintptr_t page_phys = mm_phys_alloc_page();
        _assert(mm_map_get(mm_kernel, (uintptr_t) plt + i, NULL) == MM_NADDR);
        _assert(mm_map_single(mm_kernel, (uintptr_t) plt + i, page_phys, MM_PAGE_WRITE, 0) == 0);
    }

    // Find relocation sections
    for (size_t i = 0; i < ehdr->e_shnum; ++i) {
        shdr = object_shdr(obj, i);

        if (shdr->sh_type == SHT_REL) {
            panic("TODO: support .rel sections without explicit addend\n");
        } else if (shdr->sh_type == SHT_RELA) {
            relatab = obj->image.base + shdr->sh_offset;

            for (size_t j = 0; j < shdr->sh_size / shdr->sh_entsize; ++j) {
                Elf64_Rela *rela = &relatab[j];
                Elf64_Sym *sym = &obj->image.symtab[ELF64_R_SYM(rela->r_info)];
                Elf64_Word sym_type = ELF64_ST_TYPE(sym->st_info);
                uintptr_t value = 0;

                switch (sym_type) {
                case STT_NOTYPE:
                case STT_FUNC:
                case STT_OBJECT:
                    {
                        if (sym->st_shndx == SHN_UNDEF) {
                            // External relocation against kernel or other module
                            // symbol
                            name = &obj->image.symstrtab[sym->st_name];

                            if (debug_symbol_find_by_name(name, &value) != 0) {
                                panic("Undefined reference to %s\n", name);
                            }
                            kdebug("Resolved %s: %p\n", name, value);
                        } else if (sym->st_shndx == SHN_COMMON) {
                            panic("AAAA\n");
                        } else {
                            Elf64_Shdr *tgt = object_shdr(obj, sym->st_shndx);
                            name = &obj->image.symstrtab[sym->st_name];
                            value = sym->st_value + obj->object_base + tgt->sh_addr;
                            kdebug("Internal %s: %p\n", name, value);
                        }
                    }
                    break;
                case STT_SECTION:
                    {
                        _assert(sym->st_shndx != SHN_UNDEF);
                        Elf64_Shdr *tgt = object_shdr(obj, sym->st_shndx);
                        value = tgt->sh_addr + obj->object_base;
                    }
                    break;
                default:
                    panic("Unhandled symbol type: %02x\n", sym_type);
                }

                uintptr_t S = value;
                intptr_t  A = rela->r_addend;
                uintptr_t P = rela->r_offset + obj->object_base;
                uintptr_t L, GOT;
                union {
                    int32_t word32;
                    intptr_t full;
                } relv;

                // Write relocation
                switch (ELF64_R_TYPE(rela->r_info)) {
                #define R_X86_64_64         1
                case R_X86_64_64:           // S + A
                    relv.full = S + A;
                    memcpy((void *) obj->object_base + rela->r_offset, &relv.full, 8);
                    break;
                #define R_X86_64_32         10
                case R_X86_64_32:           // S + A
                    relv.full = S + A;
                    kdebug("S = %p, A = %d\n", S, A);
                    if (!!(relv.full & 0x8000000000000000) != !!(relv.full & 0x80000000)) {
                        panic("Can't fit relocation against symbol %p: overflow\n", S);
                    }
                    relv.word32 = relv.full & 0xFFFFFFFF;
                    memcpy((void *) obj->object_base + rela->r_offset, &relv.word32, 4);
                    break;
                #define R_X86_64_PC32       2
                case R_X86_64_PC32:         // S + A - P
                    relv.full = S + A - P;
                    kdebug("S = %p, A = %d, P = %p\n", S, A, P);
                    if (!!(relv.full & 0x8000000000000000) != !!(relv.full & 0x80000000)) {
                        panic("Can't fit relocation against symbol %p: overflow\n", S);
                    }
                    relv.word32 = relv.full & 0xFFFFFFFF;
                    memcpy((void *) obj->object_base + rela->r_offset, &relv.word32, 4);
                    break;
                #define R_X86_64_PLT32      4
                case R_X86_64_PLT32:        // L + A - P
                    // Put full symbol value + trampoline into PLT
                    // jmpq *0x0(%rip)
                    plt[pltoff + 0] = 0xFF;
                    plt[pltoff + 1] = 0x25;
                    plt[pltoff + 2] = 0x00;
                    plt[pltoff + 3] = 0x00;
                    plt[pltoff + 4] = 0x00;
                    plt[pltoff + 5] = 0x00;
                    // .quad VALUE
                    memcpy(&plt[pltoff + 6], &value, sizeof(uintptr_t));

                    L = ((uintptr_t) obj->gotplt) + pltoff;
                    pltoff += 16;

                    relv.full = L + A - P;
                    kdebug("L = %p, A = %d, P = %p\n", L, A, P);
                    if (!!(relv.full & 0x8000000000000000) != !!(relv.full & 0x80000000)) {
                        panic("Can't fit relocation against symbol %p: overflow\n", S);
                    }
                    relv.word32 = relv.full & 0xFFFFFFFF;
                    memcpy((void *) obj->object_base + rela->r_offset, &relv.word32, 4);
                    break;
                #define R_X86_64_REX_GOTP   42
                case R_X86_64_REX_GOTP:     // Looks like GOT + A - P
                    memcpy(&plt[pltoff], &value, sizeof(uintptr_t));

                    GOT = ((uintptr_t) obj->gotplt) + pltoff;
                    pltoff += 16;

                    relv.full = GOT + A - P;
                    kdebug("GOT = %p, A = %d, P = %p\n", GOT, A, P);
                    if (!!(relv.full & 0x8000000000000000) != !!(relv.full & 0x80000000)) {
                        panic("Can't fit relocation against symbol %p: overflow\n", S);
                    }
                    relv.word32 = relv.full & 0xFFFFFFFF;
                    memcpy((void *) obj->object_base + rela->r_offset, &relv.word32, 4);
                    break;
                default:
                    panic("Unhandled reloc type: %02x\n", ELF64_R_TYPE(rela->r_info));
                }
            }
        }
    }

    return 0;
}

static int object_extract_info(struct object *obj) {
    const char *name, *shstrtab;
    Elf64_Ehdr *ehdr;
    Elf64_Shdr *shdr, *sh_shstrtab;

    ehdr = obj->image.base;
    sh_shstrtab = object_shdr(obj, ehdr->e_shstrndx);
    obj->image.shstrtab = obj->image.base + sh_shstrtab->sh_offset;

    // Locate symtab and strtab
    for (size_t i = 0; i < ehdr->e_shnum; ++i) {
        shdr = object_shdr(obj, i);
        name = &obj->image.shstrtab[shdr->sh_name];

        if (shdr->sh_type == SHT_STRTAB && !strcmp(name, ".strtab")) {
            obj->image.symstrtab = obj->image.base + shdr->sh_offset;
            continue;
        } else if (shdr->sh_type == SHT_SYMTAB) {
            if (obj->image.sh_symtab) {
                kwarn("Object defines several symbol tables\n");
                return -EINVAL;
            }
            obj->image.sh_symtab = shdr;
            obj->image.symtab = obj->image.base + shdr->sh_offset;
        }
    }

    if (!obj->image.symtab) {
        kerror("No symbol table in object\n");
        return -EINVAL;
    }
    if (!obj->image.symstrtab) {
        kerror("No strtab in object\n");
        return -EINVAL;
    }

    // Locate entry, exit, info symbols
    shdr = obj->image.sh_symtab;
    for (size_t i = 0; i < shdr->sh_size / shdr->sh_entsize; ++i) {
        Elf64_Sym *sym = obj->image.base + shdr->sh_offset + i * shdr->sh_entsize;
        Elf64_Word type = ELF64_ST_TYPE(sym->st_info);

        if (type == STT_FUNC || type == STT_OBJECT || type == STT_NOTYPE) {
            name = &obj->image.symstrtab[sym->st_name];
            void *value = (void *) (sym->st_value + obj->object_base);

            if (!strcmp(name, SYM_MOD_ENTER)) {
                obj->module_enter = value;
            } else if (!strcmp(name, SYM_MOD_EXIT)) {
                obj->module_exit = value;
            } else if (!strcmp(name, SYM_MOD_DESC)) {
                obj->module_desc = value;
            }
        }
    }

    if (!obj->module_desc) {
        kerror("Module defines no info struct\n");
        return -EINVAL;
    }

    if (!obj->module_enter) {
        kerror("Module defines no entry function\n");
        return -EINVAL;
    }

    return 0;
}

static int object_load(struct object *obj) {
    Elf64_Ehdr *ehdr;
    Elf64_Shdr *shdr;
    uintptr_t obj_lowest = (uintptr_t) -1;
    uintptr_t obj_highest = 0;
    int res;

    ehdr = obj->image.base;

    // Find out the object size
    for (size_t i = 0; i < ehdr->e_shnum; ++i) {
        shdr = object_shdr(obj, i);

        if (shdr->sh_flags & SHF_ALLOC) {
            if (shdr->sh_addr < obj_lowest) {
                obj_lowest = shdr->sh_addr;
            }
            if (shdr->sh_addr + shdr->sh_size > obj_highest) {
                obj_highest = shdr->sh_addr + shdr->sh_size;
            }
        }
    }
    if (obj_lowest == obj_highest) {
        // No loadable sections?
        return -EINVAL;
    }
    // Align both addresses
    obj_lowest &= ~0xFFF;
    obj_highest = (obj_highest + 0xFFF) & ~0xFFF;

    // Find GOT size from relocation count (count each reloc against
    // PLT/GOT as a single PLT entry)
    obj->gotplt_size = 0;
    for (size_t i = 0; i < ehdr->e_shnum; ++i) {
        shdr = object_shdr(obj, i);

        if (shdr->sh_type == SHT_RELA) {
            for (size_t j = 0; j < shdr->sh_size / shdr->sh_entsize; ++j) {
                Elf64_Rela *rela = obj->image.base + shdr->sh_offset + j * shdr->sh_entsize;
                Elf64_Word type = ELF64_R_TYPE(rela->r_info);

                switch (type) {
                case R_X86_64_PLT32:
                case R_X86_64_REX_GOTP:
                    obj->gotplt_size += 16;
                    break;
                default:
                    break;
                }
            }
        }
    }

    obj->object_page_count = (obj_highest - obj_lowest) / MM_PAGE_SIZE;
    obj->object_page_count += (obj->gotplt_size + MM_PAGE_SIZE - 1) / MM_PAGE_SIZE;

    // Allocate virtual memory region for the object
    obj->object_base = vmfind(mm_kernel, MODULE_MAP_START, MODULE_MAP_END, obj->object_page_count);

    if (obj->object_base == MM_NADDR) {
        return -ENOMEM;
    }

    obj->gotplt = (void *) (obj->object_base + ((obj_highest - obj_lowest) & ~0xFFF));

    // Load the sections
    for (size_t i = 0; i < ehdr->e_shnum; ++i) {
        shdr = object_shdr(obj, i);

        if (shdr->sh_flags & SHF_ALLOC && shdr->sh_size) {
            _assert(object_section_load(obj, shdr) == 0);
        }
    }

    // Extract useful info/symbols/sections
    if ((res = object_extract_info(obj)) != 0) {
        return res;
    }

    // Perform relocations
    _assert(object_reloc(obj) == 0);

    return 0;
}

int sys_module_unload(const char *name) {
    return -ENOSYS;
}

int sys_module_load(const char *_path, const char *params) {
    struct object *obj = object_create();
    int res;
    struct process *proc;
    char path[256];
    uintptr_t cr3_old, cr3;
    cr3 = MM_PHYS(mm_kernel);

    asm volatile ("movq %%cr3, %0":"=r"(cr3_old)::"memory");

    strcpy(path, _path);

    proc = thread_self->proc;
    _assert(proc);

    if (proc->ioctx.uid != 0) {
        res = -EACCES;
        goto cleanup;
    }

    // Load the image
    if ((res = object_read(obj, &proc->ioctx, path)) != 0) {
        goto cleanup;
    }

    asm volatile ("movq %0, %%cr3"::"r"(cr3):"memory");
    asm volatile ("cli");

    if ((res = object_load(obj)) != 0) {
        goto cleanup;
    }

    object_finalize_load(obj);

    debug_dump(DEBUG_DEFAULT, (void *) (obj->object_base + 0x1d), 64);

    if ((res = obj->module_enter((void *) params)) != 0) {
        goto cleanup;
    }

    // obj continues its lifetime
    asm volatile ("movq %0, %%cr3"::"r"(cr3_old):"memory");
    return 0;
cleanup:
    asm volatile ("movq %0, %%cr3"::"r"(cr3_old):"memory");
    object_free(obj);
    return res;
}

int mod_list(void *ctx, char *buf, size_t lim) {
    return -ENOSYS;
}
