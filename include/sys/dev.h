#pragma once
#include "sys/types.h"
#include "fs/node.h"

struct vnode;

enum dev_class {
    DEV_CLASS_BLOCK = 1,
    DEV_CLASS_CHAR = 2,
    DEV_CLASS_ANY = 255
};

#define DEV_BLOCK_SDx       1
#define DEV_BLOCK_HDx       2
#define DEV_BLOCK_RAM       3
#define DEV_BLOCK_CDx       4
#define DEV_BLOCK_PART      127
#define DEV_BLOCK_PSEUDO    128
#define DEV_BLOCK_OTHER     255

#define DEV_CHAR_TTY        1

int dev_add(enum dev_class cls, int subcls, void *dev, const char *name);
int dev_add_link(const char *name, struct vnode *to);
int dev_add_live_link(const char *name, vnode_link_getter_t getter);
int dev_find(enum dev_class cls, const char *name, struct vnode **dev_node);

//struct dev_entry {
//    char dev_name[64];
//
//    enum dev_class dev_class;
//    uint16_t dev_subclass;
//    uint16_t dev_number;
//
//    void *dev;
//    struct dev_entry *cdr;
//};

//struct dev_entry *dev_iter(void);

//int dev_alloc_name(enum dev_class cls, uint16_t subclass, char *name);
//int dev_by_name(struct dev_entry **ent, const char *name);
//void dev_entry_add(struct dev_entry *ent);
