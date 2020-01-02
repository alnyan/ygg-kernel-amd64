include etc/make/amd64/compiler.mk
include etc/make/amd64/platform.mk
include etc/make/amd64/config.mk

.PHONY+=$(O)/sys/amd64/hw/ap_code_blob.o

all:

# add .inc includes for asm
HEADERS+=$(shell find include -name "*.inc")

$(O)/sys/amd64/kernel.elf: $(kernel_OBJS) $(kernel_LINKER) config
	@printf " LD\t%s\n" $(@:$(O)/%=%)
	@$(CC64) $(kernel_LDFLAGS)  -o $@ $(kernel_OBJS)

$(O)/sys/%.o: sys/%.S $(HEADERS) config
	@printf " AS\t%s\n" $(@:$(O)/%=%)
	@$(CC64) $(kernel_CFLAGS) -D__ASM__ -c -o $@ $<

$(O)/sys/%.o: sys/%.c $(HEADERS) config
	@printf " CC\t%s\n" $(@:$(O)/%=%)
	@$(CC64) $(kernel_CFLAGS) -c -o $@ $<

$(O)/sys/amd64/hw/ap_code_blob.o: $(O)/sys/amd64/hw/ap_code.bin config
	@printf " AS\t%s\n" $(@:$(O)/%=%)
	@$(CC64) $(kernel_CFLAGS) -c \
		-DINCBIN_FILE='"$<"' \
		-DINCBIN_START="amd64_ap_code_start" \
		-DINCBIN_END="amd64_ap_code_end" \
		-o $@ sys/amd64/incbin.S

$(O)/sys/amd64/hw/ap_code.bin: sys/amd64/hw/ap_code.nasm config
	@printf " NASM\t%s\n" $(@:$(O)/%=%)
	@nasm -f bin -o $@ $<

$(O)/sys/font/%.o: etc/%.psfu config
	@printf " FONT\t%s\n" $(@:$(O)/%.o=%.psfu)
	@$(CC64) $(kernel_CFLAGS) -c \
		-DINCBIN_FILE='"$<"' \
		-DINCBIN_START="_psf_start" \
		-DINCBIN_END="_psf_end" \
		-o $@ sys/amd64/incbin.S

### Kernel loader build
TARGETS+=$(O)/sys/amd64/image.iso
DIRS+=$(O)/sys/amd64/loader
loader_OBJS+=$(O)/sys/amd64/loader/boot.o \
			 $(O)/sys/amd64/loader/loader.o \
			 $(O)/sys/amd64/loader/util.o
loader_LINKER=sys/amd64/loader/link.ld
loader_CFLAGS=-ffreestanding \
			  -nostdlib \
			  -I. \
			  -Iinclude \
			  -Wall \
			  -Wextra \
			  -Wpedantic \
			  -Wno-unused-argument \
			  -Werror \
			  -Wno-language-extension-token \
			  -D__KERNEL__
loader_LDFLAGS=-nostdlib -T$(loader_LINKER)

$(O)/sys/amd64/loader.elf: $(loader_OBJS) $(loader_LINKER) config
	@printf " LD\t%s\n" $(@:$(O)/%=%)
	@$(CC86) $(loader_LDFLAGS) -o $@ $(loader_OBJS)

$(O)/sys/amd64/loader/%.o: sys/amd64/loader/%.S $(HEADERS) config
	@printf " AS\t%s\n" $(@:$(O)/%=%)
	@$(CC86) $(loader_CFLAGS) -D__ASM__ -c -o $@ $<

$(O)/sys/amd64/loader/%.o: sys/amd64/loader/%.c $(HEADERS) config
	@printf " CC\t%s\n" $(@:$(O)/%=%)
	@$(CC86) $(loader_CFLAGS) -c -o $@ $<

### Initrd building
amd64_mkstage:
	@make -sC usr all O=$(O)/usr \
					  STAGE=$(O)/stage \
					  CC=$(CC64) \
					  AR=$(AR64) \
					  LD=$(LD64) \
					  S=$(abspath .)

$(O)/sys/amd64/initrd.img: amd64_mkstage
	@cd $(O)/stage && tar czf $@ *
	@du -sh $@

### Debugging and emulation
QEMU_BIN?=qemu-system-x86_64
# TODO: Uncomment these once I implement this
# This is for playing around with PCI descriptions
#QEMU_DEVS?=-device ich9-usb-uhci1,id=uhci-0 \
#		   -device usb-mouse,bus=uhci-0.0 \
#		   -device usb-kbd,bus=uhci-0.0

QEMU_CHIPSET?=q35

ifdef QEMU_HDA
QEMU_DEV_HDA=-drive file=$(QEMU_HDA),format=raw,id=disk_hda
QEMU_DEVS+=$(QEMU_DEV_HDA)
endif

ifdef QEMU_NET_TAP
QEMU_NETDEV=-netdev type=tap,id=net0
else
QEMU_NETDEV=-netdev type=user,hostfwd=tcp::5555-:22,hostfwd=udp::5555-:22,id=net0
endif

ifdef QEMU_NET
QEMU_DEV_NET=-device $(QEMU_NET),netdev=net0,mac=11:22:33:44:55:66 \
		$(QEMU_NETDEV) \
		-object filter-dump,id=f1,netdev=net0,file=eth_dump.dat
QEMU_DEVS+=$(QEMU_DEV_NET)
endif

# Use newer (ICH9-based) chipset instead of PIIX4
QEMU_OPTS?=-m $(QEMU_MEM) \
		   -chardev stdio,nowait,id=char0,mux=on \
		   -serial chardev:char0 -mon chardev=char0 \
		   --accel tcg,thread=multi \
		   -M $(QEMU_CHIPSET) \
		   -boot d \
		   $(QEMU_DEVS) \
		   -smp $(QEMU_SMP)

ifdef QEMU_DEBUG
QEMU_OPTS+=-s -S
endif

$(O)/sys/amd64/image.iso: $(O)/sys/amd64/kernel.elf \
						  $(O)/sys/amd64/loader.elf \
						  $(O)/sys/amd64/initrd.img \
						  sys/amd64/grub.cfg
	@printf " ISO\t%s\n" $(@:$(O)/%=%)
	@cp sys/amd64/grub.cfg $(O)/sys/amd64/image/boot/grub/grub.cfg
	@cp $(O)/sys/amd64/kernel.elf $(O)/sys/amd64/image/boot/kernel
	@cp $(O)/sys/amd64/loader.elf $(O)/sys/amd64/image/boot/loader
	@cp $(O)/sys/amd64/initrd.img $(O)/sys/amd64/image/boot/initrd.img
	@grub-mkrescue -o $(O)/sys/amd64/image.iso $(O)/sys/amd64/image 2>/dev/null

qemu: all
	@$(QEMU_BIN) \
		-cdrom $(O)/sys/amd64/image.iso \
		$(QEMU_OPTS)
