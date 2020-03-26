#include "sys/assert.h"
#include "sys/types.h"
#include "sys/list.h"

//void list_add(struct list *head, struct list_link *link) {
//    link->prev = head->end;
//    if (head->end) {
//        head->end->next = link;
//    } else {
//        head->begin = link;
//    }
//    link->next = NULL;
//    head->end = link;
//}
//
//void list_remove(struct list *head, struct list_link *link) {
//    struct list_link *prev, *next;
//    prev = link->prev;
//    next = link->next;
//
//    if (prev) {
//        prev->next = next;
//    } else {
//        _assert(link == head->begin);
//        head->begin = next;
//    }
//
//    if (next) {
//        next->prev = prev;
//    } else {
//        _assert(link == head->end);
//        head->end = prev;
//    }
//
//    link->prev = NULL;
//    link->next = NULL;
//}
