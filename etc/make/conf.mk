HEADERS=$(shell find ./include -name "*.h")

KERNEL_VERSION_GIT=$(shell git describe --always --tags)
KERNEL_VERSION_STR=$(KERNEL_VERSION_GIT)
# XXX: Removed until I fix SMP again
#ifeq ($(AMD64_SMP),1)
#KERNEL_VERSION_STR+= +SMP
#endif

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
	  $(O)/sys/char/input.o \
	  $(O)/sys/char/ring.o \
	  $(O)/sys/char/line.o \
	  $(O)/sys/char/tty.o \
	  $(O)/sys/char/chr.o \
	  $(O)/sys/block/pseudo.o \
	  $(O)/sys/block/part_gpt.o \
	  $(O)/sys/block/ram.o \
	  $(O)/sys/block/blk.o \
	  $(O)/sys/dev.o \
	  $(O)/sys/fs/vfs.o \
	  $(O)/sys/fs/vfs_ops.o \
	  $(O)/sys/fs/vfs_access.o \
	  $(O)/sys/fs/fs_class.o \
	  $(O)/sys/fs/node.o \
	  $(O)/sys/fs/tar.o \
	  $(O)/sys/fs/sysfs.o \
	  $(O)/sys/fs/ext2/block.o \
	  $(O)/sys/fs/ext2/ext2.o \
	  $(O)/sys/fs/ext2/node.o \
	  $(O)/sys/time.o \
	  $(O)/sys/sys_file.o \
	  $(O)/sys/sys_sys.o \
	  $(O)/sys/snprintf.o \
	  $(O)/sys/random.o \
	  $(O)/sys/init.o

OBJS+=$(O)/sys/driver/pci/pci.o \
	  $(O)/sys/driver/pci/pcidb.o \
	  $(O)/sys/driver/ata/ahci.o

DIRS+=$(O)/sys/fs \
	  $(O)/sys/fs/ext2 \
	  $(O)/sys/char \
	  $(O)/sys/block \
	  $(O)/sys/driver/pci \
	  $(O)/sys/driver/ata

ifeq ($(DEBUG_COUNTERS),1)
CFLAGS+=-DDEBUG_COUNTERS
endif
