#!/bin/sh

if [ -z "$INSTALL_HDR" ]; then
    echo "\$INSTALL_HDR is not set" >&2;
    exit 1;
fi

KERNEL_HEADERS="include/sys/fcntl.h \
                include/sys/reboot.h \
                include/sys/stat.h \
                include/sys/syscall.h \
                include/sys/fs/dirent.h \
                include/sys/types.h \
                include/sys/time.h \
                include/sys/errno.h \
                include/sys/signum.h \
                include/sys/utsname.h \
                include/sys/select.h \
                include/sys/termios.h \
                include/sys/ioctl.h"

for src_file in $KERNEL_HEADERS; do
    dst_file=$INSTALL_HDR$(echo $src_file | sed -e 's/^include//g')
    install -D -m 0644 $src_file $dst_file
done
