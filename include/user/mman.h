#pragma once
#define MAP_FAILED          ((void *) -1)
#define MAP_ANONYMOUS       (1 << 0)
#define MAP_SHARED          (1 << 1)
#define MAP_PRIVATE         (1 << 2)
#define MAP_ACCESS_MASK     (3 << 1)
#define MAP_FIXED           (1 << 3)

#define PROT_READ           (1 << 0)
#define PROT_WRITE          (1 << 1)
