#pragma once
#include "sys/types.h"

typedef int (*cfg_read_func_t)(void *ctx, char *buf, size_t lim);
typedef int (*cfg_write_func_t)(void *ctx, const char *value);

// Generic getter for config values
// ctx: a pointer to a NULL-terminated string
int sysfs_config_getter(void *ctx, char *buf, size_t lim);
// Generic (u)int64_t getter
// ctx: a pointer to an (u)int64_t value
int sysfs_config_int64_getter(void *ctx, char *buf, size_t lim);

int sysfs_add_config_endpoint(const char *name, size_t bufsz, void *ctx, cfg_read_func_t read, cfg_write_func_t write);
void sysfs_populate(void);
