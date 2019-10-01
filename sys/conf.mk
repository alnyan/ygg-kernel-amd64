HEADERS=$(shell find ./include -name "*.h")

CFLAGS+=-Wall \
		-Wextra \
		-Werror \
		-Wpedantic \
		-Wno-unused-parameter \
		-Iinclude \
		-Wno-unused-variable \
		-Wno-language-extension-token \
		-Wno-gnu-zero-variadic-macro-arguments

ifdef KERNEL_TEST_MODE
CFLAGS+=-DKERNEL_TEST_MODE
endif

DIRS+=$(O)/sys \
	  $(O)/sys/vfs \
	  $(O)/sys/vfs/ext2
OBJS+=$(O)/sys/mem.o \
	  $(O)/sys/string.o \
	  $(O)/sys/debug.o \
	  $(O)/sys/panic.o \
	  $(O)/sys/thread.o \
	  $(O)/sys/sched.o \
	  $(O)/sys/time.o \
	  $(O)/sys/blk.o \
	  $(O)/sys/vfs/node.o \
	  $(O)/sys/vfs/vfs.o \
	  $(O)/sys/vfs/fs_class.o \
	  $(O)/sys/vfs/ext2/ext2.o \
	  $(O)/sys/vfs/ext2/ext2blk.o \
	  $(O)/sys/vfs/ext2/ext2alloc.o \
	  $(O)/sys/vfs/ext2/ext2vnop.o \
	  $(O)/sys/vfs/ext2/ext2dir.o