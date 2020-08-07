# Context: O - object/output dir

ACPICA_SRC=$(shell find arch/amd64/acpica -type f -name "*.c")
ACPICA_SRC_DIR=$(shell find arch/amd64/acpica -type d)
ACPICA_DIR=$(ACPICA_SRC_DIR:%=$(O)/%)
ACPICA_OBJ=$(ACPICA_SRC:%.c=$(O)/%.o)

KERNEL_USER_HDR=$(shell find include/user -type f -name "*.h")

KERNEL_DEF=
KERNEL_OBJ=$(O)/arch/amd64/entry.o \
		   $(O)/arch/amd64/kernel.o \
		   $(O)/arch/amd64/acpi_osl_mem.o \
		   $(O)/arch/amd64/acpi_osl_printf.o \
		   $(O)/arch/amd64/acpi_osl_thread.o \
		   $(O)/arch/amd64/acpi_osl_init.o \
		   $(O)/arch/amd64/acpi_osl_table.o \
		   $(O)/arch/amd64/acpi_osl_irq.o \
		   $(O)/arch/amd64/acpi_osl_hw.o \
		   $(ACPICA_OBJ) \
		   $(O)/arch/amd64/hw/rs232.o \
		   $(O)/arch/amd64/hw/con.o \
		   $(O)/arch/amd64/hw/gdt.o \
		   $(O)/arch/amd64/hw/gdt_s.o \
		   $(O)/arch/amd64/hw/acpi.o \
		   $(O)/arch/amd64/mm/mm.o \
		   $(O)/arch/amd64/hw/apic.o \
		   $(O)/arch/amd64/hw/idt.o \
		   $(O)/arch/amd64/hw/exc_s.o \
		   $(O)/arch/amd64/hw/exc.o \
		   $(O)/arch/amd64/hw/irq0.o \
		   $(O)/arch/amd64/hw/timer.o \
		   $(O)/arch/amd64/hw/ioapic.o \
		   $(O)/arch/amd64/hw/irqs_s.o \
		   $(O)/arch/amd64/sys/spin_s.o \
		   $(O)/arch/amd64/cpu.o \
		   $(O)/arch/amd64/mm/heap.o \
		   $(O)/arch/amd64/mm/map.o \
		   $(O)/arch/amd64/mm/phys.o \
		   $(O)/arch/amd64/mm/vmalloc.o \
		   $(O)/arch/amd64/mm/pool.o \
		   $(O)/arch/amd64/hw/ps2.o \
		   $(O)/arch/amd64/hw/irq.o \
		   $(O)/arch/amd64/hw/rtc.o \
		   $(O)/arch/amd64/fpu.o \
		   $(O)/arch/amd64/cpuid.o \
		   $(O)/arch/amd64/sched_s.o \
		   $(O)/arch/amd64/syscall_s.o \
		   $(O)/arch/amd64/syscall.o \
		   $(O)/arch/amd64/binfmt_elf.o \
		   $(O)/sys/debug.o \
		   $(O)/sys/ubsan.o \
		   $(O)/sys/panic.o \
		   $(O)/sys/string.o \
		   $(O)/sys/config.o \
		   $(O)/sys/ctype.o \
		   $(O)/sys/errno.o \
		   $(O)/sys/kernel.o \
		   $(O)/sys/time.o \
		   $(O)/sys/char/input.o \
		   $(O)/sys/char/ring.o \
		   $(O)/sys/char/line.o \
		   $(O)/sys/char/tty.o \
		   $(O)/sys/char/chr.o \
		   $(O)/sys/char/pipe.o \
		   $(O)/sys/block/pseudo.o \
		   $(O)/sys/block/part_gpt.o \
		   $(O)/sys/block/ram.o \
		   $(O)/sys/block/blk.o \
		   $(O)/sys/block/cache.o \
		   $(O)/sys/execve.o \
		   $(O)/sys/dev.o \
		   $(O)/sys/sys_file.o \
		   $(O)/sys/sys_sys.o \
		   $(O)/sys/thread.o \
		   $(O)/sys/snprintf.o \
		   $(O)/sys/random.o \
		   $(O)/sys/reboot.o \
		   $(O)/sys/init.o \
		   $(O)/sys/mem/shmem.o \
		   $(O)/sys/mem/slab.o \
		   $(O)/sys/console.o \
		   $(O)/sys/display.o \
		   $(O)/sys/wait.o \
		   $(O)/sys/sched.o \
		   $(O)/sys/font/psf.o \
		   $(O)/sys/font/logo.o \
		   $(O)/sys/font/default8x16.psfu.o \
		   $(O)/sys/mod.o \
		   $(O)/fs/vfs.o \
		   $(O)/fs/vfs_ops.o \
		   $(O)/fs/vfs_access.o \
		   $(O)/fs/fs_class.o \
		   $(O)/fs/ofile.o \
		   $(O)/fs/node.o \
		   $(O)/fs/sysfs.o \
		   $(O)/fs/ram.o \
		   $(O)/fs/ram_tar.o \
		   $(O)/fs/ext2/block.o \
		   $(O)/fs/ext2/alloc.o \
		   $(O)/fs/ext2/ext2.o \
		   $(O)/fs/ext2/node.o \
		   $(O)/fs/ext2/dir.o \
		   $(O)/drivers/pci/pci.o \
		   $(O)/drivers/pci/pcidb.o \
		   $(O)/drivers/ata/ahci.o \
		   $(O)/drivers/usb/usb_uhci.o \
		   $(O)/drivers/usb/usb.o \
		   $(O)/drivers/usb/driver.o \
		   $(O)/drivers/usb/device.o \
		   $(O)/drivers/usb/usbkbd.o \
		   $(O)/drivers/usb/hub.o \

KERNEL_LDS=arch/amd64/link.ld
KERNEL_HDR=$(shell find include -type f -name "*.h")

DIRS+=$(O)/arch/amd64/hw \
	  $(O)/arch/amd64/mm \
	  $(O)/arch/amd64/sys \
	  $(O)/drivers/usb \
	  $(O)/drivers/ata \
	  $(O)/drivers/pci \
	  $(O)/fs \
	  $(O)/fs/ext2 \
	  $(O)/sys/block \
	  $(O)/sys/char \
	  $(O)/sys/mem \
	  $(O)/sys/font \
	  $(O)/net \
	  $(O)/include \
	  $(ACPICA_DIR)

include etc/KernelOptions.makefile

### Compiler

KERNEL_GIT_VERSION=$(shell git describe --always --tags)
KERNEL_CFLAGS=-Iinclude \
			  -Iinclude/arch/amd64/acpica \
			  -I$(O)/include \
			  -ffreestanding \
			  -fPIE \
			  -fno-plt \
			  -fno-pic \
			  -DARCH_AMD64 \
			  -DKERNEL_VERSION_STR=\"$(KERNEL_GIT_VERSION)\" \
			  -D__KERNEL__ \
			  -mcmodel=large \
			  -mno-sse \
			  -mno-sse2 \
			  -mno-red-zone \
			  -mno-mmx \
			  -z max-page-size=0x1000 \
			  -Wall \
			  -Wextra \
			  -Wno-unused \
			  -O0 \
			  -ggdb \
			  -Werror $(KERNEL_DEF)
KERNEL_LDFLAGS=-nostdlib \
			   -fPIE \
			   -fno-plt \
			   -fno-pic \
			   -static \
			   -Wl,--build-id=none \
			   -z max-page-size=0x1000 \
			   -T$(KERNEL_LDS)

### Rules

$(O)/kernel: $(KERNEL_LDS) $(KERNEL_OBJ)
	@echo " LD  $@"
	@$(CC) $(KERNEL_LDFLAGS) -o $@ $(KERNEL_OBJ)

$(O)/include/config.h: config.h.in
	@echo " CONFIG $@"
	@cp $< $@

$(O)/%.o: %.c $(O)/include/config.h $(KERNEL_HDR) Kernel.config
	@echo " CC  $@"
	@$(CC) $(KERNEL_CFLAGS) -c -o $@ $<

$(O)/%.o: %.S $(O)/include/config.h $(KERNEL_HDR) Kernel.config
	@echo " AS  $@"
	@$(CC) $(KERNEL_CFLAGS) -D__ASM__ -c -o $@ $<

$(O)/sys/font/default8x16.psfu.o: etc/default8x16.psfu arch/amd64/incbin.S
	@echo " RES $@"
	@$(CC) \
		-c \
		-D__ASM__ \
		-DINCBIN_FILE='"etc/default8x16.psfu"' \
		-DINCBIN_START="_psf_start" \
		-DINCBIN_END="_psf_end" \
		-o $@ arch/amd64/incbin.S
