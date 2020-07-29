#pragma once

struct vfs_ioctx;
struct process;
struct ofile;

int elf_load(struct process *proc, struct vfs_ioctx *ctx, struct ofile *fd, uintptr_t *entry);
int binfmt_is_elf(const char *guess, size_t len);
int elf_is_dynamic(struct vfs_ioctx *ioctx, struct ofile *fd, int *is_dynamic);
