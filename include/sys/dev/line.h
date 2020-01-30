#pragma once
struct chrdev;

// Common TTY line discipline
ssize_t line_read(struct chrdev *chr, void *buf, size_t pos, size_t lim);

// Simple line discipline - no control, disregard termios, spit out data right after
// a single byte is available
ssize_t simple_line_read(struct chrdev *chr, void *buf, size_t pos, size_t lim);

// TODO: invent some magic for devices like /dev/urandom and /dev/zero etc.
//       they're implemented as block devices right now
