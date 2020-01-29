#include "sys/amd64/loader/multiboot.h"
#include "sys/amd64/loader/util.h"
#include "sys/amd64/loader/data.h"
#include "sys/amd64/loader/elf.h"

#define KERNEL_VIRT_BASE        0xFFFFFF0000000000

extern void long_entry(uint64_t v);

struct amd64_loader_data loader_data;
uint64_t pml4[512] __attribute__((aligned(0x1000)));
static uint64_t loader_pdpt[512] __attribute__((aligned(0x1000)));
static uint64_t kernel_pdpt[512] __attribute__((aligned(0x1000)));

static uintptr_t modules_end = 0;
static uintptr_t loader_end;
extern uintptr_t _loader_end;   // Linker

static void paging_init(void) {
    // 0x0000000000000000 - 0x0000000040000000 - loader, 1GiB
    pml4[0] = (uint64_t) (uintptr_t) loader_pdpt | 1 | 2;
    loader_pdpt[0] = 1 | 2 | (1 << 7);

    // 0xFFFFFF0000000000 - 0xFFFFFF0040000000 - kernel, 1GiB
    pml4[(KERNEL_VIRT_BASE & 0xFFFFFFFFFFFF) >> 39] = (uint64_t) (uintptr_t) kernel_pdpt | 1 | 2;
    kernel_pdpt[0] = 1 | 2 | (1 << 7);
}

static uint64_t elf_load(uintptr_t src) {
    Elf64_Ehdr *ehdr = (Elf64_Ehdr *) src;

    // Validate magic
    if (ehdr->e_ident[EI_MAG0] != ELFMAG0 ||
        ehdr->e_ident[EI_MAG1] != ELFMAG1 ||
        ehdr->e_ident[EI_MAG2] != ELFMAG2 ||
        ehdr->e_ident[EI_MAG3] != ELFMAG3) {
        panic("Invalid ELF magic");
    }

    // Validate target platform
    if (ehdr->e_ident[EI_CLASS] != ELFCLASS64) {
        panic("This image was not intended to run on amd64");
    }

    Elf64_Shdr *shdrs = (Elf64_Shdr *) (src + (uintptr_t) ehdr->e_shoff);
    const char *shstrtabd = (const char *) (src + (uintptr_t) shdrs[ehdr->e_shstrndx].sh_offset);

    loader_data.strtab_ptr = 0;
    loader_data.strtab_size = 0;
    loader_data.symtab_ptr = 0;
    loader_data.symtab_size = 0;

    for (size_t i = 0; i < (size_t) ehdr->e_shnum; ++i) {
        Elf64_Shdr *shdr = &shdrs[i];
        const char *name = &shstrtabd[shdr->sh_name];

        if (shdr->sh_type == SHT_SYMTAB && !strcmp(name, ".symtab")) {
            loader_data.symtab_ptr = (uint32_t) shdr->sh_offset + src;
            loader_data.symtab_size = (uint32_t) shdr->sh_size;
        }

        if (shdr->sh_type == SHT_STRTAB && !strcmp(name, ".strtab")) {
            puts("Strtab section: ");
            puts(name);
            putc('\n');

            loader_data.strtab_ptr = (uint32_t) shdr->sh_offset + src;
            loader_data.strtab_size = (uint32_t) shdr->sh_size;
        }

        if (shdr->sh_flags & SHF_ALLOC) {
            if (!strcmp(name, ".note.gnu.property")) {
                // Fuck you, gcc
                continue;
            }

            uint64_t phys_addr = shdr->sh_addr - KERNEL_VIRT_BASE;
            puts("Section ");
            puts(name);
            putc('\n');
            puts("\tPhysical address: ");
            putx(phys_addr);
            putc('\n');

            if (phys_addr < loader_end || phys_addr < modules_end) {
                panic("Physical load address is too low: may overwrite modules");
            }

            if (shdr->sh_type == SHT_NOBITS) {
                memset((void *) (uintptr_t) phys_addr, 0, (size_t) shdr->sh_size);
            } else if (shdr->sh_type == SHT_PROGBITS) {
                memcpy((void *) (uintptr_t) phys_addr, (const void *) (src + (uintptr_t) shdr->sh_offset), (size_t) shdr->sh_size);
            }
        }
    }

    puts("Entry: ");
    putx(ehdr->e_entry);
    putc('\n');

    return ehdr->e_entry;
}

void loader_main(uint32_t magic, struct multiboot_info *mb_info) {
    if (magic != MULTIBOOT_BOOTLOADER_MAGIC) {
        panic("Invalid bootloader magic");
    }

    // Setup loader_data
    loader_data.multiboot_info_ptr = (uintptr_t) mb_info;

    struct multiboot_mod_list *mod_list = (struct multiboot_mod_list *) (uintptr_t) mb_info->mods_addr;
    char *kernel_cmdline = NULL;
    uintptr_t kernel_mod = 0;
    uintptr_t initrd_mod = 0;

    loader_data.initrd_ptr = 0;
    loader_data.initrd_len = 0;

    if (mb_info->mods_count == 1) {
        // Only kernel is provided, ignore cmdline
        kernel_mod = mod_list[0].mod_start;
    } else if (mb_info->mods_count == 2) {
        // initrd and kernel provided
        // Find out which is which

        for (size_t i = 0; i < mb_info->mods_count; ++i) {
            if (!strncmp((const char *) (uintptr_t) mod_list[i].cmdline, "kernel", 6)) {
                if (kernel_mod) {
                    panic("Two \"kernel\"s provided!");
                }
                kernel_mod = mod_list[i].mod_start;
                kernel_cmdline = (char *) (uintptr_t) mod_list[i].cmdline;
            } else {
                if (initrd_mod) {
                    panic("Two \"initrd\"s provided!");
                }
                initrd_mod = mod_list[i].mod_start;

                loader_data.initrd_len = mod_list[i].mod_end - mod_list[i].mod_start;
                loader_data.initrd_ptr = mod_list[i].mod_start;
            }
        }
    } else {
        panic("More than two modules provided");
    }

    if (!kernel_mod) {
        panic("No kernel to load");
    }

    // Calculate boundaries so we don't overwrite something accidentally
    for (size_t i = 0; i < mb_info->mods_count; ++i) {
        if (mod_list[i].mod_end > modules_end) {
            modules_end = mod_list[i].mod_end;
        }
    }

    if (kernel_cmdline) {
        if (strlen(kernel_cmdline) >= KERNEL_CMDLINE_MAX) {
            kernel_cmdline[KERNEL_CMDLINE_MAX - 1] = 0;
        }
        strcpy(loader_data.cmdline, kernel_cmdline);

        puts("Kernel cmdline: ");
        puts(kernel_cmdline);
        putc('\n');
    } else {
        loader_data.cmdline[0] = 0;
    }
    puts("Modules end addr: ");
    putx(modules_end);
    putc('\n');
    loader_end = (uintptr_t) &_loader_end;
    puts("Loader end addr: ");
    putx(loader_end);
    putc('\n');

    paging_init();

    uint64_t entry = elf_load(mod_list[0].mod_start);

    // Calculate checksum for loader_data
    uint8_t checksum = 0;
    for (size_t i = 0; i < sizeof(struct amd64_loader_data) - sizeof(uint8_t); ++i) {
        checksum += ((uint8_t *) &loader_data)[i];
    }
    loader_data.checksum = -checksum;

    long_entry(entry);
}
