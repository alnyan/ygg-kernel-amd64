#include "sys/amd64/loader/multiboot.h"
#include "sys/amd64/asm/asm_irq.h"
#include "sys/amd64/hw/pci/pci.h"
#include "sys/amd64/hw/rs232.h"
#include "sys/amd64/smp/smp.h"
#include "sys/amd64/syscall.h"
#include "sys/amd64/mm/phys.h"
#include "sys/amd64/hw/acpi.h"
#include "sys/amd64/hw/apic.h"
#include "sys/amd64/hw/gdt.h"
#include "sys/amd64/hw/idt.h"
#include "sys/amd64/hw/ps2.h"
#include "sys/amd64/hw/rtc.h"
#include "sys/amd64/hw/con.h"
#include "sys/amd64/cpuid.h"
#include "sys/amd64/mm/mm.h"
#include "sys/amd64/cpu.h"
#include "sys/amd64/fpu.h"
#include "sys/fs/ofile.h"
#include "sys/fs/fcntl.h"
#include "sys/fs/node.h"
#include "sys/dev/tty.h"
#include "sys/dev/ram.h"
#include "sys/dev/dev.h"
#include "sys/fs/tar.h"
#include "sys/thread.h"
#include "sys/fs/vfs.h"
#include "sys/config.h"
#include "sys/string.h"
#include "sys/assert.h"
#include "sys/errno.h"
#include "sys/panic.h"
#include "sys/debug.h"
#include "sys/sched.h"
#include "sys/heap.h"
#include "sys/time.h"

static multiboot_info_t *multiboot_info;
struct thread user_init = {0};

extern int sys_execve(const char *path, const char **argp, const char **envp);

static void user_init_func(void *arg) {
    kdebug("Starting user init\n");

    struct vfs_ioctx *ioctx = &thread_self->ioctx;
    struct vnode *root_dev, *tty_dev;
    int res;
    // Mount root
    if ((res = dev_find(DEV_CLASS_BLOCK, "ram0", &root_dev)) != 0) {
        kerror("ram0: %s\n", kstrerror(res));
        panic("Fail\n");
    }

    if ((res = vfs_mount(ioctx, "/", root_dev->dev, "ustar", NULL)) != 0) {
        kerror("mount: %s\n", kstrerror(res));
        panic("Fail\n");
    }

    // Open STDOUT_FILENO and STDERR_FILENO
    struct ofile *fd_stdout = kmalloc(sizeof(struct ofile));
    struct ofile *fd_stdin = kmalloc(sizeof(struct ofile));

    if ((res = dev_find(DEV_CLASS_CHAR, "tty0", &tty_dev)) != 0) {
        kerror("tty0: %s\n", kstrerror(res));
        panic("Fail\n");
    }

    if ((res = vfs_open_vnode(ioctx, fd_stdin, tty_dev, O_RDONLY)) != 0) {
        kerror("tty0: %s\n", kstrerror(res));
        panic("Fail\n");
    }

    if ((res = vfs_open_vnode(ioctx, fd_stdout, tty_dev, O_WRONLY)) != 0) {
        kerror("tty0: %s\n", kstrerror(res));
        panic("Fail\n");
    }

    thread_self->fds[0] = fd_stdin;
    thread_self->fds[1] = fd_stdout;
    thread_self->fds[2] = fd_stdout;
    // Duplicate the FD
    ++fd_stdout->refcount;

    _assert(fd_stdin->refcount == 1);
    _assert(fd_stdout->refcount == 2);

    sys_execve("/test", NULL, NULL);

    while (1) {
        asm volatile ("hlt");
    }
}

void kernel_main(struct amd64_loader_data *data) {
    cpuid_init();
    data = (struct amd64_loader_data *) MM_VIRTUALIZE(data);
    multiboot_info = (multiboot_info_t *) MM_VIRTUALIZE(data->multiboot_info_ptr);

    if (data->symtab_ptr) {
        kinfo("Kernel symbol table at %p\n", MM_VIRTUALIZE(data->symtab_ptr));
        debug_symbol_table_set(MM_VIRTUALIZE(data->symtab_ptr), MM_VIRTUALIZE(data->strtab_ptr), data->symtab_size, data->strtab_size);
    }

    // Parse kernel command line
    kernel_set_cmdline(data->cmdline);

    // Reinitialize RS232 properly
    amd64_con_init();
    rs232_init(RS232_COM1);
    ps2_init();

    amd64_phys_memory_map((multiboot_memory_map_t *) MM_VIRTUALIZE(multiboot_info->mmap_addr),
                          multiboot_info->mmap_length);

    amd64_gdt_init();
    amd64_idt_init(0);
    amd64_mm_init(data);

    ps2_register_device();

    amd64_acpi_init();

    // Print kernel version now
    kinfo("yggdrasil " KERNEL_VERSION_STR "\n");

    amd64_apic_init();
    rtc_init();
    // Setup system time
    struct tm t;
    rtc_read(&t);
    system_boot_time = mktime(&t);
    kinfo("Boot time: %04u-%02u-%02u %02u:%02u:%02u\n",
        t.tm_year, t.tm_mon, t.tm_mday,
        t.tm_hour, t.tm_min, t.tm_sec);

    pci_init();

    vfs_init();
    tty_init();
    if (data->initrd_ptr) {
        // Create ram0 block device
        ramblk_init(MM_VIRTUALIZE(data->initrd_ptr), data->initrd_len);
        tarfs_init();
    }

    syscall_init();

    sched_init();

    thread_init(&user_init, (uintptr_t) user_init_func, NULL, 0);
    user_init.pid = thread_alloc_pid(0);
    sched_queue(&user_init);

    sched_enter();

    panic("This code should not run\n");
}
