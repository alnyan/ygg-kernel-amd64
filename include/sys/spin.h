#pragma once
#include "sys/types.h"

#if defined(ARCH_AMD64)
#include "arch/amd64/sys/spin.h"
#endif

void spin_lock(spin_t *s);
void spin_release(spin_t *s);
void spin_lock_irqsave(spin_t *s, uintptr_t *irq);
void spin_release_irqrestore(spin_t *s, uintptr_t *irq);
