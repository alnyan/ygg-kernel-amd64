### This file contains all things for a generic amd64-build

### Kernel build
DEFINES+=-DARCH_AMD64

ACPICA_SRCS=$(shell find arch/amd64/acpica -type f -name "*.c")
ACPICA_OBJS=$(ACPICA_SRCS:%.c=$(O)/%.o)
ACPICA_SRCD=$(shell find arch/amd64/acpica -type d)
ACPICA_OBJD=$(ACPICA_SRCD:%=$(O)/%)

OBJS+=$(ACPICA_OBJS) \
	  $(O)/arch/amd64/acpi_osl_mem.o \
	  $(O)/arch/amd64/acpi_osl_printf.o \
	  $(O)/arch/amd64/acpi_osl_thread.o \
	  $(O)/arch/amd64/acpi_osl_init.o \
	  $(O)/arch/amd64/acpi_osl_table.o \
	  $(O)/arch/amd64/acpi_osl_irq.o \
	  $(O)/arch/amd64/acpi_osl_hw.o \
	  $(O)/arch/amd64/hw/rs232.o \
	  $(O)/arch/amd64/kernel.o \
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
	  $(O)/arch/amd64/hw/con.o

kernel_LINKER=arch/amd64/link.ld
kernel_LDFLAGS=-nostdlib \
			   -fPIE \
			   -fno-plt \
			   -fno-pic \
			   -static \
			   -Wl,--build-id=none \
			   -z max-page-size=0x1000 \
			   -ggdb \
			   -T$(kernel_LINKER)
kernel_CFLAGS_BASE=-ffreestanding \
			  	   -I. \
			  	   -I include/arch/amd64/acpica \
			  	   $(DEFINES) \
			  	   $(CFLAGS) \
			  	   -nostdlib \
			  	   -fPIE \
			  	   -fno-plt \
			  	   -fno-pic \
			  	   -static \
			  	   -fno-asynchronous-unwind-tables \
			  	   -Wno-format \
			  	   -Wno-comment \
			  	   -Wno-unused-but-set-variable \
			  	   -mcmodel=large \
			  	   -mno-red-zone \
			  	   -mno-mmx \
			  	   -mno-sse \
			  	   -mno-sse2 \
			  	   -z max-page-size=0x1000 \
			  	   -D__KERNEL__ \
				   -fstack-protector-all \
				   -fstack-protector-strong
kernel_CFLAGS=-fsanitize=undefined \
			  -fsanitize-address-use-after-scope \
			  $(kernel_CFLAGS_BASE)

kernel_OBJS=$(O)/arch/amd64/crti.o \
			$(O)/arch/amd64/entry.o \
			$(shell $(CC64) $(CFLAGS) -print-file-name=crtbegin.o) \
			$(OBJS) \
			$(shell $(CC64) $(CFLAGS) -print-file-name=crtend.o) \
			$(O)/arch/amd64/crtn.o

DIRS+=$(O)/arch/amd64/image/boot/grub \
	  $(O)/arch/amd64/hw/pci \
	  $(O)/arch/amd64/hw \
	  $(O)/arch/amd64/sys \
	  $(O)/arch/amd64/mm \
	  $(ACPICA_OBJD)
