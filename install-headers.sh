#!/bin/sh

if [ -z "$INSTALL_HDR" ]; then
    echo "\$INSTALL_HDR is not set" >&2;
    exit 1;
fi

KERNEL_HEADERS="include/user/fcntl.h \
                include/user/errno.h \
                include/user/time.h \
                include/user/dirent.h \
                include/user/ioctl.h \
                include/user/stat.h \
                include/user/statvfs.h \
                include/user/mount.h \
                include/user/syscall.h \
                include/user/termios.h \
                include/user/types.h \
                include/user/signum.h \
                include/user/utsname.h \
                include/user/netctl.h \
                include/user/socket.h \
                include/user/inet.h \
                include/user/mman.h \
                include/user/reboot.h"

for src_file in $KERNEL_HEADERS; do
    dst_file=$INSTALL_HDR$(echo $src_file | sed -e 's/^include\/user\//\/ygg\//g')
    install -D -m 0644 $src_file $dst_file
done
