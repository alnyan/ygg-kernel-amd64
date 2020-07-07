#pragma once
#include "sys/types.h"
#include "sys/list.h"

struct sockaddr;
struct socket;

struct sockops {
    ssize_t (*recvfrom) (struct socket *, void *, size_t, struct sockaddr *, size_t *);
    ssize_t (*sendto) (struct socket *, const void *, size_t, struct sockaddr *, size_t);

    int (*open) (struct socket *);
    void (*close) (struct socket *);

    int (*bind) (struct socket *, struct sockaddr *, size_t);
    int (*connect) (struct socket *, struct sockaddr *, size_t);
    int (*setsockopt) (struct socket *, int, void *, size_t);
    int (*accept) (struct socket *, struct socket *);

    int (*count_pending) (struct socket *);
};

struct socket_class {
    const char *name;
    int domain, type;
    int (*supports) (int proto);

    struct sockops *ops;

    struct list_head link;
};

void socket_class_register(struct socket_class *cls);
