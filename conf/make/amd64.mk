all:
### Kernel build
OBJS+=

### Kernel loader build
TARGETS+=$(O)/loader.elf
DIRS+=$(O)/arch/amd64/loader
loader_OBJS+=$(O)/arch/amd64/loader/boot.o \
			 $(O)/arch/amd64/loader/loader.o
loader_LINKER=$(S)/arch/amd64/loader/link.ld
loader_CFLAGS=-ffreestanding -nostdlib -I$(S) -m32
loader_LDFLAGS=-nostdlib -melf_i386 -T$(loader_LINKER)

$(O)/loader.elf: $(loader_OBJS) $(HEADERS) $(loader_LINKER)
	@printf " LD\t%s\n" $@
	@$(CROSSLD) $(loader_LDFLAGS) -o $@ $(loader_OBJS)

$(O)/arch/amd64/loader/%.o: $(S)/arch/amd64/loader/%.S
	@printf " AS\t%s\n" $@
	@$(CROSSCC) $(loader_CFLAGS) -c -o $@ $<

$(O)/arch/amd64/loader/%.o: $(S)/arch/amd64/loader/%.c
	@printf " CC\t%s\n" $@
	@$(CROSSCC) $(loader_CFLAGS) -c -o $@ $<
