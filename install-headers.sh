#!/bin/sh

if [ -z "$INSTALL_HDR" ]; then
    echo "\$INSTALL_HDR is not set" >&2;
    exit 1;
fi

KERNEL_HEADERS="include/sys/user/fcntl.h \
                include/sys/user/errno.h \
                include/sys/user/time.h \
                include/sys/user/dirent.h \
                include/sys/user/ioctl.h \
                include/sys/user/stat.h \
                include/sys/user/statvfs.h \
                include/sys/user/syscall.h \
                include/sys/user/termios.h \
                include/sys/user/types.h \
                include/sys/user/signum.h \
                include/sys/user/utsname.h \
                include/sys/user/reboot.h"

for src_file in $KERNEL_HEADERS; do
    dst_file=$INSTALL_HDR$(echo $src_file | sed -e 's/^include//g')
    install -D -m 0644 $src_file $dst_file
done
