#pragma once
#include "list.h"

typedef struct tree {
    void *value;
    struct tree *parent;
    list_simple_t *children;
} tree_t;
