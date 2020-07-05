#pragma once

struct vfs_ioctx;
struct process;
struct ofile;

int elf_load(struct process *proc, struct vfs_ioctx *ctx, struct ofile *fd, uintptr_t *entry);
