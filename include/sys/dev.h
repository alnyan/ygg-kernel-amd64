#pragma once
#include "sys/types.h"

// TODO: something like device name alias
//       for example, /dev/root aliased to
//       /dev/ramN or /dev/sdXN
enum dev_class {
    DEV_CLASS_BLOCK,
    DEV_CLASS_CHAR
};

#define DEV_BLOCK_SDx       1
#define DEV_BLOCK_HDx       2
#define DEV_BLOCK_RAM       3
#define DEV_BLOCK_PART      127
#define DEV_BLOCK_PSEUDO    128

struct dev_entry {
    char dev_name[64];

    enum dev_class dev_class;
    uint16_t dev_subclass;
    uint16_t dev_number;

    void *dev;
    struct dev_entry *cdr;
};

struct dev_entry *dev_iter(void);

int dev_alloc_name(enum dev_class cls, uint16_t subclass, char *name);
int dev_by_name(struct dev_entry **ent, const char *name);
void dev_entry_add(struct dev_entry *ent);
