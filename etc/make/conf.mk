HEADERS=$(shell find ./include -name "*.h")

KERNEL_VERSION_GIT=$(shell git describe --always --tags)
KERNEL_VERSION_STR=$(KERNEL_VERSION_GIT)
ifeq ($(AMD64_SMP),1)
KERNEL_VERSION_STR+= +SMP
endif

OPTIMIZE?=g

CFLAGS+=-Wall \
		-Wextra \
		-Werror \
		-Wno-unused-parameter \
		-Iinclude \
		-Wno-unused-variable \
		-Wno-language-extension-token \
		-Wno-gnu-zero-variadic-macro-arguments \
		-O$(OPTIMIZE) \
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
	  $(O)/sys/block/cache.o \
	  $(O)/sys/kernel.o \
	  $(O)/sys/dev.o \
	  $(O)/sys/time.o \
	  $(O)/sys/sys_file.o \
	  $(O)/sys/sys_sys.o \
	  $(O)/sys/snprintf.o \
	  $(O)/sys/random.o \
	  $(O)/sys/reboot.o \
	  $(O)/sys/init.o \
	  $(O)/sys/font/logo.o \
	  $(O)/sys/font/psf.o \
	  $(O)/sys/font/default8x16.o \
	  $(O)/sys/list.o
DIRS+=$(O)/sys \
	  $(O)/sys/char \
	  $(O)/sys/block \
	  $(O)/sys/font

OBJS+=$(O)/fs/vfs.o \
	  $(O)/fs/vfs_ops.o \
	  $(O)/fs/vfs_access.o \
	  $(O)/fs/fs_class.o \
	  $(O)/fs/node.o \
	  $(O)/fs/tar.o \
	  $(O)/fs/sysfs.o \
	  $(O)/fs/ext2/block.o \
	  $(O)/fs/ext2/ext2.o \
	  $(O)/fs/ext2/node.o
DIRS+=$(O)/fs \
	  $(O)/fs/ext2

OBJS+=$(O)/drivers/pci/pci.o \
	  $(O)/drivers/pci/pcidb.o \
	  $(O)/drivers/ata/ahci.o \
	  $(O)/drivers/usb/usb_uhci.o \
	  $(O)/drivers/usb/usb.o \
	  $(O)/drivers/usb/driver.o \
	  $(O)/drivers/usb/device.o \
	  $(O)/drivers/usb/usbkbd.o \
	  $(O)/drivers/usb/hub.o
DIRS+=$(O)/drivers/pci \
	  $(O)/drivers/ata \
	  $(O)/drivers/usb

ifeq ($(DEBUG_COUNTERS),1)
CFLAGS+=-DDEBUG_COUNTERS
endif
