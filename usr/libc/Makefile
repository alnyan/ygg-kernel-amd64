libc_OBJS=$(O)/libc/syscall.o

libc_CRTI=$(O)/libc/crti.o
libc_CRTN=$(O)/libc/crtn.o
libc_CRT0=$(O)/libc/crt0.o

libc_DIRS=$(O)/libc
libc_CFLAGS=-nostdlib \
			-ffreestanding \
			-Iinclude \
			-mno-sse \
			-mno-sse2 \
			-I$(S)/include

all: mkdirs $(libc_CRTI) $(libc_CRTN) $(libc_CRT0) $(O)/libc.a

mkdirs:
	mkdir -p $(libc_DIRS)

$(O)/libc.a: $(libc_OBJS)
	$(AR) rcs $@ $(libc_OBJS)

$(O)/libc/%.o: %.c
	$(CC) -c -o $@ $(libc_CFLAGS) $<

$(O)/libc/%.o: %.S
	$(CC) -c -o $@ $(libc_CFLAGS) $<
