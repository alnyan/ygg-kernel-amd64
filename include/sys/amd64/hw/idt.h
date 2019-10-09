#pragma once
#include "sys/types.h"

struct amd64_idtr {
    uint16_t size;
    uintptr_t offset;
} __attribute__((packed));

extern const struct amd64_idtr amd64_idtr;

void amd64_idt_init(void);
