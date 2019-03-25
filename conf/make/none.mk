HEADERS=$(shell find $(S) -name "*.h")

CFLAGS+=-Wall \
		-Wextra \
		-Werror \
		-Wpedantic \
		-Wno-unused-parameter

DIRS+=$(O)/sys
OBJS+=$(O)/sys/mem.o \
	  $(O)/sys/string.o \
	  $(O)/sys/debug.o
