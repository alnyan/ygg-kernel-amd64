HEADERS=$(shell find ./include -name "*.h")

KERNEL_VERSION_GIT=$(shell git describe --always --tags)
KERNEL_VERSION_STR=$(KERNEL_VERSION_GIT)
ifeq ($(AMD64_SMP),1)
KERNEL_VERSION_STR+= +SMP
endif

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

ifeq ($(ALL_SETUID_0),1)
CFLAGS+=-DALL_SETUID_0
endif

CFLAGS+=-D__KERNEL__ \
		-DKERNEL_VERSION_STR='"$(KERNEL_VERSION_STR)"'

OBJS+=$(O)/sys/debug.o \
	  $(O)/sys/string.o \
	  $(O)/sys/panic.o \
	  $(O)/sys/errno.o \
	  $(O)/sys/config.o \
	  $(O)/sys/ctype.o \
	  $(O)/sys/sched.o \
	  $(O)/sys/thread.o \
	  $(O)/sys/dev/input.o \
	  $(O)/sys/dev/ring.o \
	  $(O)/sys/dev/tty.o \
	  $(O)/sys/dev/ram.o \
	  $(O)/sys/dev/chr.o \
	  $(O)/sys/dev/blk.o \
	  $(O)/sys/dev/dev.o \
	  $(O)/sys/dev/line.o \
	  $(O)/sys/fs/vfs.o \
	  $(O)/sys/fs/vfs_ops.o \
	  $(O)/sys/fs/vfs_access.o \
	  $(O)/sys/fs/fs_class.o \
	  $(O)/sys/fs/node.o \
	  $(O)/sys/fs/pseudo.o \
	  $(O)/sys/fs/tar.o \
	  $(O)/sys/fs/sysfs.o \
	  $(O)/sys/time.o \
	  $(O)/sys/sys_file.o

DIRS+=$(O)/sys/fs \
	  $(O)/sys/dev

ifeq ($(DEBUG_COUNTERS),1)
CFLAGS+=-DDEBUG_COUNTERS
endif
