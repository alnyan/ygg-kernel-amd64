all:
### Kernel build
DEFINES+=-DARCH_AMD64
OBJS+=$(O)/arch/amd64/kernel.o \
	  $(O)/arch/amd64/mm/pool.o \
	  $(O)/arch/amd64/mm/mm.o \
	  $(O)/arch/amd64/hw/rs232.o \
	  $(O)/arch/amd64/hw/gdt.o \
	  $(O)/arch/amd64/hw/gdt_s.o \
	  $(O)/arch/amd64/hw/idt.o \
	  $(O)/arch/amd64/hw/ints.o \
	  $(O)/arch/amd64/hw/exc.o \
	  $(O)/arch/amd64/hw/regs.o \
	  $(O)/arch/amd64/hw/irq0.o \
	  $(O)/arch/amd64/hw/pic8259.o \
	  $(O)/arch/amd64/hw/irqs.o \
	  $(O)/arch/amd64/mm/map.o \
	  $(O)/arch/amd64/hw/timer.o \
	  $(O)/arch/amd64/acpi/tables.o \
	  $(O)/arch/amd64/acpi/hpet.o \
	  $(O)/arch/amd64/mm/phys.o \
	  $(O)/arch/amd64/mm/heap.o
kernel_OBJS=$(O)/arch/amd64/entry.o \
			$(OBJS)
kernel_LINKER=$(S)/arch/amd64/link.ld
kernel_LDFLAGS=-nostdlib \
			   -fPIE \
			   -fno-plt \
			   -fno-pic \
			   -static \
			   -Wl,--build-id=none \
			   -z max-page-size=0x1000 \
			   -T$(kernel_LINKER)
kernel_CFLAGS=-ffreestanding \
			  -I. \
			  $(DEFINES) \
			  $(CFLAGS) \
			  -fPIE \
			  -fno-plt \
			  -fno-pic \
			  -static \
			  -fno-asynchronous-unwind-tables \
			  -mcmodel=large \
			  -mno-red-zone \
			  -mno-mmx \
			  -mno-sse \
			  -mno-sse2 \
			  -z max-page-size=0x1000 \
			  -m64
DIRS+=$(O)/arch/amd64/mm \
	  $(O)/arch/amd64/hw \
	  $(O)/arch/amd64/acpi
# add .inc includes for asm
HEADERS+=$(shell find $(S) -name "*.inc")

$(O)/kernel.elf: $(kernel_OBJS) $(kernel_LINKER)
	@printf " LD\t%s\n" $@
	@$(CROSSCC) $(kernel_LDFLAGS)  -o $@ $(kernel_OBJS)

$(O)/%.o: $(S)/%.S $(HEADERS)
	@printf " AS\t%s\n" $@
	@$(CROSSCC) $(kernel_CFLAGS) -c -o $@ $<

$(O)/%.o: $(S)/%.c $(HEADERS)
	@printf " CC\t%s\n" $@
	@$(CROSSCC) $(kernel_CFLAGS) -c -o $@ $<

### Kernel loader build
TARGETS+=$(O)/loader.elf $(O)/kernel.elf
DIRS+=$(O)/arch/amd64/loader
loader_OBJS+=$(O)/arch/amd64/loader/boot.o \
			 $(O)/arch/amd64/loader/loader.o \
			 $(O)/arch/amd64/loader/util.o
loader_LINKER=$(S)/arch/amd64/loader/link.ld
loader_CFLAGS=-ffreestanding \
			  -nostdlib \
			  -I. \
			  -Iinclude \
			  -m32 \
			  -Wall \
			  -Wextra \
			  -Wpedantic \
			  -Wno-unused-argument \
			  -Werror \
			  -Wno-language-extension-token
loader_LDFLAGS=-nostdlib -melf_i386 -T$(loader_LINKER)

$(O)/loader.elf: $(loader_OBJS) $(loader_LINKER)
	@printf " LD\t%s\n" $@
	@$(CROSSLD) $(loader_LDFLAGS) -o $@ $(loader_OBJS)

$(O)/arch/amd64/loader/%.o: $(S)/arch/amd64/loader/%.S $(HEADERS)
	@printf " AS\t%s\n" $@
	@$(CROSSCC) $(loader_CFLAGS) -c -o $@ $<

$(O)/arch/amd64/loader/%.o: $(S)/arch/amd64/loader/%.c $(HEADERS)
	@printf " CC\t%s\n" $@
	@$(CROSSCC) $(loader_CFLAGS) -c -o $@ $<

### Debugging and emulation
QEMU_BIN?=qemu-system-x86_64
QEMU_OPTS?=-nographic \
		   -serial mon:stdio \
		   -m 512

qemu: all
	@$(QEMU_BIN) \
		-kernel $(O)/loader.elf \
		-initrd $(O)/kernel.elf $(QEMU_OPTS)
