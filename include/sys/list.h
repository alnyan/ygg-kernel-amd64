#pragma once

struct list_link {
    struct list_link *prev, *next;
};

struct list {
    struct list_link *begin, *end;
};

#define LIST_HEAD(name) \
    struct list name = { NULL, NULL }

#define list_foreach(head, iter) \
    for (struct list_link *iter = (head)->begin; iter; iter = iter->next)

#define list_link_value(link, type, member)     ({ \
        const typeof (((type *) 0)->member) *__memb = (link); \
        (type *) ((char *) __memb - offsetof(type, member)); \
    })

#define list_link_init(link) ({ \
        (link)->prev = NULL; \
        (link)->next = NULL; \
    })

void list_append(struct list *list, struct list_link *ptr);
void list_remove(struct list *list, struct list_link *ptr);
