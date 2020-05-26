CC64?=x86_64-elf-gcc
AR64?=x86_64-elf-ar
LD64?=x86_64-elf-ld

ifeq (,$(shell which $(CC64)))
$(error Failed to find "$${CC64}": $(CC64))
endif
ifeq (,$(shell which $(LD64)))
	$(error Failed to find "$${LD64}": $(LD64))
endif
