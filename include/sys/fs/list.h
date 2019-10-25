#pragma once

typedef struct list_simple {
    void *car;
    struct list_simple *cdr;
} list_simple_t;

