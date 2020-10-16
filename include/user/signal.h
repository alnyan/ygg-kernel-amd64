#pragma once

struct user_stack {
    void *ss_sp;
    size_t ss_size;
};

struct ucontext {
    struct ucontext *uc_link;
    struct user_stack *uc_stack;
};

// TODO: move this typedef to user headers
#if !defined(__KERNEL__)
typedef struct user_stack stack_t;
#endif
