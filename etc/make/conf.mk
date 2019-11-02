HEADERS=$(shell find ./include -name "*.h")

CFLAGS+=-Wall \
		-Wextra \
		-Werror \
		-Wno-unused-parameter \
		-Iinclude \
		-Wno-unused-variable \
		-Wno-language-extension-token \
		-Wno-gnu-zero-variadic-macro-arguments \
		-O0 \
		-ggdb

ifdef KERNEL_TEST_MODE
CFLAGS+=-DKERNEL_TEST_MODE
endif

# DIRS+=$(O)/sys \
# 	  $(O)/sys/vfs \
# 	  $(O)/sys/vfs/ext2 \
# 	  $(O)/sys/blk
# OBJS+=$(O)/sys/mem.o \
# 	  $(O)/sys/string.o \
# 	  $(O)/sys/debug.o \
# 	  $(O)/sys/panic.o \
# 	  $(O)/sys/thread.o \
# 	  $(O)/sys/sched.o \
# 	  $(O)/sys/time.o \
# 	  $(O)/sys/blk.o \
# 	  $(O)/sys/vfs/node.o \
# 	  $(O)/sys/vfs/vfs.o \
# 	  $(O)/sys/vfs/fs_class.o \
# 	  $(O)/sys/vfs/ext2/ext2.o \
# 	  $(O)/sys/vfs/ext2/ext2blk.o \
# 	  $(O)/sys/vfs/ext2/ext2alloc.o \
# 	  $(O)/sys/vfs/ext2/ext2vnop.o \
# 	  $(O)/sys/vfs/ext2/ext2dir.o \
# 	  $(O)/sys/blk/ram.o \
# 	  $(O)/sys/vfs/tar.o \
# 	  $(O)/sys/binfmt_elf.o \
# 	  $(O)/sys/errno.o \
# 	  $(O)/sys/tty.o \
# 	  $(O)/sys/chr.o

OBJS+=$(O)/sys/debug.o \
	  $(O)/sys/string.o \
	  $(O)/sys/panic.o \
	  $(O)/sys/errno.o \
	  $(O)/sys/blk.o \
	  $(O)/sys/dev.o \
	  $(O)/sys/tty.o \
	  $(O)/sys/chr.o \
	  $(O)/sys/vfs/fs_class.o \
	  $(O)/sys/vfs/node.o \
	  $(O)/sys/vfs/vfs.o \
	  $(O)/sys/vfs/ext2/ext2alloc.o \
	  $(O)/sys/vfs/ext2/ext2blk.o \
	  $(O)/sys/vfs/ext2/ext2.o \
	  $(O)/sys/vfs/ext2/ext2dir.o \
	  $(O)/sys/vfs/ext2/ext2vnop.o \
	  $(O)/sys/vfs/tar.o \
	  $(O)/sys/blk/ram.o \
	  $(O)/sys/ring.o \
	  $(O)/sys/net/eth.o \
	  $(O)/sys/net/arp.o \
	  $(O)/sys/net/in.o
DIRS+=$(O)/sys/vfs/ext2 \
	  $(O)/sys/net \
	  $(O)/sys/blk
