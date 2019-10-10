#pragma once

#if defined(ARCH_AMD64)
#include "sys/amd64/sys/spin.h"
#endif

void spin_lock(spin_t *s);
void spin_release(spin_t *s);
