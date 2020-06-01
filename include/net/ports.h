#pragma once
#include "sys/types.h"
#include "sys/list.h"

#define PORT_ARRAY_BUCKET_COUNT             64

struct port_array_entry {
    uint16_t port_no;
    void *socket;
    struct list_head list;
};

struct port_array {
    struct list_head buckets[PORT_ARRAY_BUCKET_COUNT];
};

void port_array_init(struct port_array *arr);
int port_array_lookup(struct port_array *arr, uint16_t port_no, void **port);
void port_array_insert(struct port_array *arr, uint16_t port_no, void *port);
void *port_array_delete(struct port_array *arr, uint16_t key);
