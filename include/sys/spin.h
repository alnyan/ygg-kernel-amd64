#pragma once
#include "sys/types.h"

#if defined(ARCH_AMD64)
#include "arch/amd64/sys/spin.h"
#endif

#if defined(AMD64_SMP)
void spin_lock(spin_t *s);
void spin_release(spin_t *s);
void spin_lock_irqsave(spin_t *s, uintptr_t *irq);
void spin_release_irqrestore(spin_t *s, uintptr_t *irq);
#else
#define spin_lock(s)
#define spin_release(s)
#define spin_lock_irqsave(s, i)
#define spin_release_irqrestore(s, i)
#endif
