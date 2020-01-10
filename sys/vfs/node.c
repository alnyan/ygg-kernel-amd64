#include "sys/fs/node.h"
#include "sys/fs/vfs.h"
#include "sys/fs/fs.h"

#include "sys/debug.h"
#include "sys/assert.h"
#include "sys/heap.h"
#include "sys/string.h"

struct vnode *vnode_create(const char *name) {
    struct vnode *node = (struct vnode *) kmalloc(sizeof(struct vnode));
    _assert(node);

    node->parent = NULL;
    node->first_child = NULL;
    node->next_child = NULL;

    if (name) {
        _assert(strlen(name) < NODE_MAXLEN);
        strcpy(node->name, name);
    } else {
        node->name[0] = 0;
    }

    return node;
}
