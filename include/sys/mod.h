#pragma once
#include "sys/types.h"

int sys_module_load(const char *path, const char *params);
int sys_module_unload(const char *name);

int mod_list(void *ctx, char *buf, size_t lim);
