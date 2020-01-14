#pragma once
#include "sys/thread.h"

int elf_load(struct thread *thr, struct vfs_ioctx *ctx, struct ofile *fd);
