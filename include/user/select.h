#pragma once

struct timeval;

// 64 bits
typedef long int __fd_mask;

#define __NFDBITS       (8 * (int) sizeof(__fd_mask))
// TODO: Guess this should be the same as the maximum
//       per-process filedes count
#define __FD_SETSIZE    64

typedef struct {
    __fd_mask fds_bits[__FD_SETSIZE / __NFDBITS];
} fd_set;

#define FD_SET(fd, fds)     \
    (fds)->fds_bits[(fd) / __NFDBITS] |= (1ULL << ((fd) % __NFDBITS))

#define FD_ISSET(fd, fds)   \
    (!!((fds)->fds_bits[(fd) / __NFDBITS] & (1ULL << ((fd) % __NFDBITS))))

#define FD_ZERO(fds)        \
    for (int __i = 0; __i < (__FD_SETSIZE / __NFDBITS); ++__i) \
        (fds)->fds_bits[__i] = 0

