#pragma once

struct ofile;
struct vnode;

int pipe_create(struct ofile **read, struct ofile **write);
int pipe_fifo_create(struct vnode *res);
