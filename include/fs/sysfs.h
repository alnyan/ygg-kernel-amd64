#pragma once
#include "sys/types.h"

#define sysfs_buf_puts(buf, lim, s) \
    { \
        if (lim > 0) { \
            size_t l = MIN(strlen(s), (lim) - 1); \
            strncpy(buf, s, l); \
            buf += l; \
            *buf = 0; \
            lim -= l; \
        } \
    }
#define sysfs_buf_printf(buf, lim, fmt, ...) \
    { \
        int res = snprintf(buf, lim, fmt, ##__VA_ARGS__); \
        if (res >= (int) lim) { \
            lim = 0; \
        } else { \
            buf += res; \
            lim -= res; \
        } \
    }

#define SYSFS_MODE_DEFAULT      0644

struct vnode;

typedef int (*cfg_read_func_t)(void *ctx, char *buf, size_t lim);
typedef int (*cfg_write_func_t)(void *ctx, const char *value);

// Generic getter for config values
// ctx: a pointer to a NULL-terminated string
int sysfs_config_getter(void *ctx, char *buf, size_t lim);
// Generic (u)int64_t getter
// ctx: a pointer to an (u)int64_t value
int sysfs_config_int64_getter(void *ctx, char *buf, size_t lim);

int sysfs_del_ent(struct vnode *dir);

int sysfs_add_dir(struct vnode *at, const char *path, struct vnode **result);
int sysfs_add_config_endpoint(struct vnode *at, const char *name, mode_t mode, size_t bufsz, void *ctx, cfg_read_func_t read, cfg_write_func_t write);
void sysfs_populate(void);
