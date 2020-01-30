#pragma once

struct vfs_ioctx;
struct thread;
struct ofile;

int elf_load(struct thread *thr, struct vfs_ioctx *ctx, struct ofile *fd, uintptr_t *entry);
