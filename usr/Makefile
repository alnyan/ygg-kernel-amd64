.PHONY: libc

# Input parameters:
# O -- build files
# STAGE -- initrd rootfs

DIRS=$(O)
STAGE_BIN=$(STAGE)/init

usr_CFLAGS=-ffreestanding \
		   -nostdlib \
		   -mno-sse \
		   -mno-sse2 \
		   -Ilibc/include \
		   -I$(S)/include
usr_LDFLAGS=-nostdlib \
			-lgcc \
			-Tlibc/program.ld
usr_STATIC_LIBS=$(O)/libc.a
usr_CRTI=$(O)/libc/crti.o
usr_CRTN=$(O)/libc/crtn.o
usr_CRT0=$(O)/libc/crt0.o

sys_CRTBEGIN=$(shell $(CC) $(CFLAGS) -print-file-name=crtbegin.o)
sys_CRTEND=$(shell $(CC) $(CFLAGS) -print-file-name=crtend.o)

all: mkdirs libc mkstage-etc $(STAGE_BIN)

$(STAGE)/init: init.c
	$(CC) -o $@ $(usr_CFLAGS) $(usr_LDFLAGS) \
		$(usr_CRTI) \
		$(sys_CRTBEGIN) \
		$(usr_CRT0) \
		$< \
		$(sys_CRTEND) \
		$(usr_CRTN) \
		$(usr_STATIC_LIBS)

libc:
	@make -sC libc all

mkdirs:
	mkdir -p $(DIRS)

mkstage-etc:
	cp -r etc $(STAGE)
