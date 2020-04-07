include etc/make/amd64/compiler.mk
include etc/make/amd64/platform.mk
include etc/make/amd64/config.mk

.PHONY+=$(O)/arch/amd64/hw/ap_code_blob.o

all:

TARGETS+=$(O)/loader.elf $(O)/kernel.elf

# add .inc includes for asm
HEADERS+=$(shell find include -name "*.inc")

$(O)/kernel.elf: $(kernel_OBJS) $(kernel_LINKER) config
	@printf " LD\t%s\n" $(@:$(O)/%=%)
	@$(CC64) $(kernel_LDFLAGS)  -o $@ $(kernel_OBJS)

$(O)/%.o: %.S $(HEADERS) config
	@printf " AS\t%s\n" $(@:$(O)/%=%)
	@$(CC64) $(kernel_CFLAGS) -D__ASM__ -c -o $@ $<

$(O)/%.o: %.c $(HEADERS) config
	@printf " CC\t%s\n" $(@:$(O)/%=%)
	@$(CC64) $(kernel_CFLAGS) -c -o $@ $<

$(O)/arch/amd64/acpica/%.o: arch/amd64/acpica/%.c
	@printf " CC\t%s\n" $(@:$(O)/%=%)
	@$(CC64) $(kernel_CFLAGS_BASE) -c -o $@ $<

$(O)/arch/amd64/hw/ap_code_blob.o: $(O)/arch/amd64/hw/ap_code.bin config
	@printf " AS\t%s\n" $(@:$(O)/%=%)
	@$(CC64) $(kernel_CFLAGS) -c \
		-DINCBIN_FILE='"$<"' \
		-DINCBIN_START="amd64_ap_code_start" \
		-DINCBIN_END="amd64_ap_code_end" \
		-o $@ arch/amd64/incbin.S

$(O)/arch/amd64/hw/ap_code.bin: arch/amd64/hw/ap_code.nasm config
	@printf " NASM\t%s\n" $(@:$(O)/%=%)
	@nasm -f bin -o $@ $<

$(O)/sys/font/%.o: etc/%.psfu config
	@printf " FONT\t%s\n" $(@:$(O)/%.o=%.psfu)
	@$(CC64) $(kernel_CFLAGS) -c \
		-DINCBIN_FILE='"$<"' \
		-DINCBIN_START="_psf_start" \
		-DINCBIN_END="_psf_end" \
		-o $@ arch/amd64/incbin.S

### Kernel loader build
DIRS+=$(O)/arch/amd64/loader
loader_OBJS+=$(O)/arch/amd64/loader/boot.o \
			 $(O)/arch/amd64/loader/loader.o \
			 $(O)/arch/amd64/loader/util.o
loader_LINKER=arch/amd64/loader/link.ld
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

$(O)/loader.elf: $(loader_OBJS) $(loader_LINKER) config
	@printf " LD\t%s\n" $(@:$(O)/%=%)
	@$(CC86) $(loader_LDFLAGS) -o $@ $(loader_OBJS)

$(O)/arch/amd64/loader/%.o: arch/amd64/loader/%.S $(HEADERS) config
	@printf " AS\t%s\n" $(@:$(O)/%=%)
	@$(CC86) $(loader_CFLAGS) $(DEFINES) -D__ASM__ -c -o $@ $<

$(O)/arch/amd64/loader/%.o: arch/amd64/loader/%.c $(HEADERS) config
	@printf " CC\t%s\n" $(@:$(O)/%=%)
	@$(CC86) $(loader_CFLAGS) $(DEFINES) -c -o $@ $<
