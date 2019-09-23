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

DIRS+=$(O)/sys
OBJS+=$(O)/sys/mem.o \
	  $(O)/sys/string.o \
	  $(O)/sys/debug.o \
	  $(O)/sys/panic.o \
	  $(O)/sys/thread.o \
	  $(O)/sys/sched.o \
	  $(O)/sys/time.o
