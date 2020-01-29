include etc/make/amd64/compiler.mk
include etc/make/amd64/platform.mk
include etc/make/amd64/config.mk

.PHONY+=$(O)/sys/amd64/hw/ap_code_blob.o

all:

TARGETS+=$(O)/sys/amd64/loader.elf $(O)/sys/amd64/kernel.elf

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

### Kernel loader build
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
	@$(CC86) $(loader_CFLAGS) $(DEFINES) -D__ASM__ -c -o $@ $<

$(O)/sys/amd64/loader/%.o: sys/amd64/loader/%.c $(HEADERS) config
	@printf " CC\t%s\n" $(@:$(O)/%=%)
	@$(CC86) $(loader_CFLAGS) $(DEFINES) -c -o $@ $<
