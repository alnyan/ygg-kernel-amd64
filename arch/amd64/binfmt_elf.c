#include "arch/amd64/mm/map.h"
#include "sys/binfmt_elf.h"
#include "sys/mem/phys.h"
#include "user/fcntl.h"
#include "user/errno.h"
#include "fs/vfs.h"
#include "sys/assert.h"
#include "sys/thread.h"
#include "sys/string.h"
#include "sys/debug.h"
#include "sys/panic.h"
#include "sys/heap.h"
#include "sys/elf.h"

#define ELF_ADDR_MIN 0x400000

// uintptr_t amd64_map_get(const mm_space_t space, uintptr_t vaddr, uint64_t *flags);
// int amd64_map_single(mm_space_t pml4, uintptr_t virt_addr, uintptr_t phys, uint32_t flags);

static int elf_map_region(mm_space_t space, uintptr_t vma_dst, size_t size) {
    uintptr_t page_aligned = vma_dst & ~0xFFF;
    uintptr_t page_offset = vma_dst & 0xFFF;
    size_t npages = (size + page_offset + 4095) / 4096;
    uintptr_t page_phys;

    for (size_t i = 0; i < npages; ++i) {
        // TODO: access flags (e.g. is section writable?)
        if ((page_phys = mm_map_get(space, page_aligned + i * MM_PAGE_SIZE, NULL)) == MM_NADDR) {
            // Allocation needed
            assert((page_phys = mm_phys_alloc_page(PU_PRIVATE)) != MM_NADDR,
                    "Failed to allocate memory\n");
            assert(mm_map_single(space, page_aligned + i * MM_PAGE_SIZE, page_phys, MM_PAGE_USER | MM_PAGE_WRITE) == 0,
                    "Failed to map memory\n");
        }
    }

    return 0;
}

static int elf_read(struct vfs_ioctx *ctx, struct ofile *fd, off_t pos, void *dst, size_t count) {
    off_t res;
    ssize_t bread;

    if ((res = vfs_lseek(ctx, fd, pos, SEEK_SET)) != pos) {
        return res;
    }

    if ((bread = vfs_read(ctx, fd, dst, count)) != (ssize_t) count) {
        panic("TODO: properly handle this case\n");
    }

    return 0;
}

static int elf_load_bytes(mm_space_t space, struct vfs_ioctx *ctx, struct ofile *fd, uintptr_t vma_dst, off_t load_src, size_t size) {
    uintptr_t page_aligned = vma_dst & ~0xFFF;
    uintptr_t page_offset = vma_dst & 0xFFF;
    size_t npages = (size + page_offset + 4095) / 4096;
    uintptr_t page_phys;
    size_t rem = size;
    size_t off = 0;
    char buf[4096];
    int res;

    for (size_t i = 0; i < npages; ++i) {
        assert((page_phys = mm_map_get(space, page_aligned + i * MM_PAGE_SIZE, NULL)), "What?");
        size_t nbytes = MIN(rem, 4096 - page_offset);

        if ((res = elf_read(ctx, fd, load_src + off, buf, nbytes)) != 0) {
            return res;
        }

        memcpy((void *) MM_VIRTUALIZE(page_phys + page_offset), buf, nbytes);
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

    for (size_t i = 0; i < npages; ++i) {
        assert((page_phys = mm_map_get(space, page_aligned + i * MM_PAGE_SIZE, NULL)), "What?");
        size_t nbytes = MIN(rem, 4096 - page_offset);
        memset((void *) MM_VIRTUALIZE(page_phys + page_offset), 0, nbytes);
        rem -= nbytes;
        page_offset = 0;
    }

    return 0;
}

int binfmt_is_elf(const char *ident, size_t len) {
    if (len < 4) {
        return 0;
    }
    return !strncmp(ident, "\x7F""ELF", 4);
}

int elf_is_dynamic(struct vfs_ioctx *ioctx, struct ofile *fd, int *is_dynamic) {
    Elf64_Ehdr ehdr;
    Elf64_Phdr phdr;
    int res;

    *is_dynamic = 0;

    if ((res = elf_read(ioctx, fd, 0, &ehdr, sizeof(Elf64_Ehdr))) != 0) {
        return res;
    }

    if (ehdr.e_ident[EI_CLASS] != ELFCLASS64) {
        return -ENOEXEC;
    }

    for (size_t i = 0; i < ehdr.e_phnum; ++i) {
        if ((res = elf_read(ioctx,
                            fd,
                            ehdr.e_phoff + i * ehdr.e_phentsize,
                            &phdr,
                            ehdr.e_phentsize)) != 0) {
            return res;
        }

        if (phdr.p_type == PT_DYNAMIC) {
            *is_dynamic = 1;
            break;
        }
    }

    return 0;
}

int elf_load(struct process *proc, struct vfs_ioctx *ctx, struct ofile *fd, uintptr_t *entry) {
    int res;
    ssize_t bread;
    Elf64_Ehdr ehdr;
    Elf64_Phdr phdr;

    if ((res = elf_read(ctx, fd, 0, &ehdr, sizeof(Elf64_Ehdr))) != 0) {
        kerror("elf: failed to read file header\n");
        return res;
    }

    // Check magic
    if (strncmp((const char *) ehdr.e_ident, "\x7F""ELF", 4) != 0) {
        kerror("elf: magic mismatch\n");
        return -EINVAL;
    }

    if (ehdr.e_ident[EI_CLASS] != ELFCLASS64) {
        kerror("elf: object was not intended for 64-bit\n");
        return -EINVAL;
    }

    _assert(ehdr.e_phnum);

    proc->image_end = 0;
    proc->brk = 0;

    // Load program segments
    for (size_t i = 0; i < ehdr.e_phnum; ++i) {
        size_t off = ehdr.e_phoff + ehdr.e_phentsize * i;

        if ((res = elf_read(ctx, fd, off, &phdr, ehdr.e_phentsize)) != 0) {
            kerror("elf: failed to read program header\n");
        }

        if (phdr.p_type == PT_LOAD) {
            uintptr_t start_aligned = phdr.p_vaddr & ~0xFFF;
            size_t size_aligned = ((phdr.p_vaddr + phdr.p_memsz + 0xFFF) & ~0xFFF) - start_aligned;

            //kdebug("[%2d] vaddr=%p\n", i, phdr.p_vaddr);

            if (elf_map_region(proc->space, start_aligned, size_aligned) != 0) {
                panic("Failed to map segment\n");
            }

            if (elf_load_bytes(proc->space, ctx, fd, phdr.p_vaddr, phdr.p_offset, phdr.p_filesz) != 0) {
                panic("Failed to load filesz data\n");
            }
            if (phdr.p_memsz > phdr.p_filesz) {
                _assert(elf_bzero(proc->space, phdr.p_vaddr + phdr.p_filesz, phdr.p_memsz - phdr.p_filesz) == 0);
            }
        }
    }

    proc->brk = (proc->image_end + MM_PAGE_SIZE - 1) & ~MM_PAGE_OFFSET_MASK;

    *entry = ehdr.e_entry;
    res = 0;

    return res;
}
