#pragma once
#include "sys/thread.h"

int elf_load(struct thread *thr, const void *eptr);
