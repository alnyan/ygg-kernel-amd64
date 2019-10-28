libc_OBJS=$(O)/libc/syscall.o
libc_DIRS=$(O)/libc
libc_CFLAGS=-nostdlib \
			-ffreestanding \
			-Iinclude \
			-mno-sse \
			-mno-sse2 \
			-I$(S)/include

all: mkdirs $(O)/libc.a

mkdirs:
	mkdir -p $(libc_DIRS)

$(O)/libc.a: $(libc_OBJS)
	$(AR) rcs $@ $(libc_OBJS)

$(O)/libc/%.o: %.c
	$(CC) -c -o $@ $(libc_CFLAGS) $<
