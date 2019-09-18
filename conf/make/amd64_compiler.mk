
CC86?=i686-elf-gcc
LD86?=i686-elf-ld

CC64?=x86_64-elf-gcc
LD64?=x86_64-elf-ld

# Check that the tools exist
ifeq (,$(shell which $(CC86)))
$(error Failed to find "$${CC86}": $(CC86))
endif
ifeq (,$(shell which $(LD86)))
	$(error Failed to find "$${LD86}": $(LD86))
endif
ifeq (,$(shell which $(CC64)))
$(error Failed to find "$${CC64}": $(CC64))
endif
ifeq (,$(shell which $(LD64)))
	$(error Failed to find "$${LD64}": $(LD64))
endif
