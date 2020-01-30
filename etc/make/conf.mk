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
	  $(O)/sys/time.o

ifeq ($(DEBUG_COUNTERS),1)
CFLAGS+=-DDEBUG_COUNTERS
endif
