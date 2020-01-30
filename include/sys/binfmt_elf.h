#pragma once

struct thread;

int elf_load(struct thread *thr, const void *src, uintptr_t *entry);
