#include "sys/mm.h"
#include "sys/debug.h"
#include "sys/panic.h"
#include "sys/amd64/hw/gdt.h"
#include "sys/amd64/hw/pic8259.h"
#include "sys/amd64/hw/ints.h"
#include "sys/amd64/hw/timer.h"
#include "sys/amd64/loader/data.h"
#include "sys/amd64/loader/multiboot.h"
#include "sys/amd64/mm/phys.h"
#include "sys/amd64/acpi/tables.h"
#include "sys/amd64/mm/pool.h"
#include "sys/sched.h"
#include "sys/string.h"
#include "sys/assert.h"
#include "sys/heap.h"
#include "sys/fs/tar.h"
#include "sys/fs/vfs.h"
#include "sys/errno.h"
#include "sys/fs/fcntl.h"
#include "sys/blk/ram.h"
#include "sys/thread.h"
#include "sys/binfmt_elf.h"
#include "sys/kidle.h"

// TODO: move to some util header
#define __wfe() asm volatile ("sti; hlt")

// For the sake of log readability
#define kernel_startup_section(text) \
    kdebug("\n"); \
    kdebug("====== " text "\n"); \
    kdebug("\n")

static struct amd64_loader_data *loader_data = 0;
static multiboot_info_t *multiboot_info;
static struct vfs_ioctx kernel_ioctx = { NULL, 0, 0 };

static uint8_t amd64_loader_data_checksum(const struct amd64_loader_data *ld) {
    uint8_t chk = 0;
    for (size_t i = 0; i < sizeof(struct amd64_loader_data); ++i) {
        chk += ((uint8_t *) ld)[i];
    }
    return chk;
}

static void amd64_loader_data_process(void) {
    // Virtualize pointer locations, as the loader has passed their
    // physical locations
    multiboot_info = (multiboot_info_t *) MM_VIRTUALIZE(loader_data->multiboot_info_ptr);
}

static int init_start(void) {
    struct stat st;
    int res;

    if ((res = vfs_stat(&kernel_ioctx, "/etc/init", &st)) < 0) {
        return res;
    }

    if ((st.st_mode & S_IFMT) != S_IFREG) {
        kerror("/etc/init: not a regular file\n");
        return -EINVAL;
    }

    if (!(st.st_mode & 0111)) {
        kerror("/etc/init: no one can execute this\n");
        return -EINVAL;
    }

    kdebug("/etc/init is %S\n", st.st_size);

    // That seems to be a weird way of loading binaries, whatever
    void *exec_buf = kmalloc(st.st_size);
    _assert(exec_buf);
    struct ofile fd;
    char buf[512];
    size_t pos = 0;
    ssize_t bread;

    if ((res = vfs_open(&kernel_ioctx, &fd, "/etc/init", 0, O_RDONLY)) < 0) {
        return res;
    }

    while ((bread = vfs_read(&kernel_ioctx, &fd, buf, 512)) > 0) {
        memcpy((void *) ((uintptr_t) exec_buf + pos), buf, bread);
        pos += bread;
    }

    vfs_close(&kernel_ioctx, &fd);

    kdebug("Successfully read init file\n");

    thread_t *init_thread = (thread_t *) kmalloc(sizeof(thread_t));
    _assert(init_thread);
    mm_space_t thread_space = amd64_mm_pool_alloc();
    _assert(thread_space);
    mm_space_clone(thread_space, mm_kernel, MM_CLONE_FLG_KERNEL);

    if (thread_init(init_thread, thread_space, 0, 0, 0, 0, 0, 0) < 0) {
        panic("Failed to set init up\n");
    }

    if ((res = elf_load(init_thread, exec_buf)) < 0) {
        return res;
    }

    sched_add(init_thread);

    return 0;
}

void kernel_main(uintptr_t loader_info_phys_ptr) {
    kdebug("Booting\n");

#if defined(KERNEL_TEST_MODE)
    kdebug("Kernel testing mode enabled\n");
#endif

    // Obtain boot information from loader
    loader_data = (struct amd64_loader_data *) MM_VIRTUALIZE(loader_info_phys_ptr);
    if (amd64_loader_data_checksum(loader_data)) {
        panic("Loader data checksum is invalid\n");
    }
    // Process loader data
    amd64_loader_data_process();

    // Memory management
    kernel_startup_section("Memory management");
    amd64_phys_memory_map((multiboot_memory_map_t *) MM_VIRTUALIZE(multiboot_info->mmap_addr), multiboot_info->mmap_length);
    amd64_mm_init(loader_data);

    kernel_startup_section("Basic hardware");
    amd64_gdt_init();
    pic8259_init();
    acpi_tables_init();
    amd64_timer_configure();
    amd64_idt_init();

#if defined(KERNEL_TEST_MODE)
    kernel_startup_section("Test dumps");
    mm_describe(mm_kernel);
#endif

    extern void amd64_setup_syscall(void);
    amd64_setup_syscall();

    // Setup /dev/ram0
    // As we don't yet support booting with rootfs on disk
    if (!loader_data->initrd_len) {
        panic("Failed to init /dev/ram0: no initrd\n");
    }

    kdebug("initrd: %p, %S\n", loader_data->initrd_ptr, loader_data->initrd_len);
    ramblk_init(MM_VIRTUALIZE(loader_data->initrd_ptr), loader_data->initrd_len);

    // Setup tarfs class
    tarfs_init();

    // Mount tarfs@ram0 as root
    if (vfs_mount(&kernel_ioctx, "/", ramblk0, "ustar", NULL) < 0) {
        panic("Failed to mount root\n");
    }

    // Try to execute an ELF binary from initrd
    int res;
    if ((res = init_start()) < 0) {
        kerror("Failed to execute init: %s\n", kstrerror(res));
    }

    kidle_init();

    // Wait until entering [kidle]
    while (1) {
        __wfe();
    }
}
