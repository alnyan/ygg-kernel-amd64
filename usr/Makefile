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
usr_LDFLAGS=-nostdlib
usr_STATIC_LIBS=$(O)/libc.a

all: mkdirs libc mkstage-etc $(STAGE_BIN)

$(STAGE)/init: init.c
	$(CC) -o $@ $(usr_CFLAGS) $(usr_LDFLAGS) $< $(usr_STATIC_LIBS)

libc:
	@make -sC libc all

mkdirs:
	mkdir -p $(DIRS)

mkstage-etc:
	cp -r etc $(STAGE)
