HEADERS=$(shell find ./include -name "*.h")

KERNEL_VERSION_GIT=$(shell git describe --always --tags)

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
		-DKERNEL_VERSION_STR=\"$(KERNEL_VERSION_GIT)\"

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
# 	  $(O)/sys/chr.o

OBJS+=$(O)/sys/debug.o \
	  $(O)/sys/string.o \
	  $(O)/sys/panic.o \
	  $(O)/sys/errno.o \
	  $(O)/sys/vfs/fs_class.o \
	  $(O)/sys/chr.o \
	  $(O)/sys/blk.o \
	  $(O)/sys/dev.o \
	  $(O)/sys/vfs/node.o \
	  $(O)/sys/vfs/vfs.o \
	  $(O)/sys/ring.o \
	  $(O)/sys/net/eth.o \
	  $(O)/sys/net/arp.o \
	  $(O)/sys/net/in.o \
	  $(O)/sys/net/netdev.o \
	  $(O)/sys/vfs/pseudo.o \
	  $(O)/sys/vfs/tar.o \
	  $(O)/sys/blk/ram.o \
	  $(O)/sys/blk/part_gpt.o \
 	  $(O)/sys/tty.o \
	  $(O)/sys/reboot.o \
	  $(O)/sys/random.o \
	  $(O)/sys/init.o \
	  $(O)/sys/vfs/vfs_ops.o \
	  $(O)/sys/vfs/vfs_access.o \
	  $(O)/sys/time.o \
	  $(O)/sys/thread.o \
	  $(O)/sys/ctype.o \
	  $(O)/sys/line.o \
	  $(O)/sys/config.o \
	  $(O)/sys/input.o \
	  $(O)/sys/vfs/sysfs.o \
	  $(O)/sys/vfs/ext2/ext2.o \
	  $(O)/sys/vfs/ext2/block.o \
	  $(O)/sys/vfs/ext2/node.o

USB_ENABLE?=1
ifeq ($(USB_ENABLE),1)
CFLAGS+=-DUSB_ENABLE
OBJS+=$(O)/sys/usb/device.o \
	  $(O)/sys/usb/driver.o \
	  $(O)/sys/usb/usbkbd.o \
	  $(O)/sys/usb/usb.o
endif
# \
	  $(O)/sys/vfs/pty.o \

ifeq ($(VESA_ENABLE),1)
OBJS+=$(O)/sys/psf.o \
	  $(O)/sys/font/default8x16.o \
	  $(O)/sys/font/logo.o
endif

DIRS+=$(O)/sys/vfs/ext2 \
	  $(O)/sys/net \
	  $(O)/sys/blk \
	  $(O)/sys/usb \
	  $(O)/sys/vfs/ext2

ifeq ($(DEBUG_COUNTERS),1)
CFLAGS+=-DDEBUG_COUNTERS
endif
