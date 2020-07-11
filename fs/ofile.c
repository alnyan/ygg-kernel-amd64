#include <config.h>
#include "sys/mem/slab.h"
#include "sys/assert.h"
#include "fs/ofile.h"
#include "fs/vfs.h"

static struct slab_cache *ofile_cache = NULL;

struct ofile *ofile_create(void) {
    struct ofile *of;

    if (!ofile_cache) {
        ofile_cache = slab_cache_get(sizeof(struct ofile));
    }

    // Also sets refcount to zero
    of = slab_calloc(ofile_cache);
    _assert(of);
    return of;
}

void ofile_destroy(struct ofile *of) {
    _assert(!of->refcount);
    slab_free(ofile_cache, of);
}

void ofile_close(struct vfs_ioctx *ioctx, struct ofile *of) {
    // An ofile should be open before closing
    _assert(of->refcount > 0);
    --of->refcount;
    if (of->refcount == 0) {
#if defined(ENABLE_NET)
        if (of->flags & OF_SOCKET) {
            net_close(ioctx, of);
        } else
#endif
        {
            vfs_close(ioctx, of);
        }
        ofile_destroy(of);
    }
}

struct ofile *ofile_dup(struct ofile *of) {
    _assert(of);
    _assert(of->refcount >= 0);
    ++of->refcount;
    return of;
}
