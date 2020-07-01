#pragma once

struct ofile;

int pipe_create(struct ofile **read, struct ofile **write);
