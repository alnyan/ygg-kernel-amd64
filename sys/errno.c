#include "sys/errno.h"

const char *kstrerror(int e) {
    switch (e) {
    case -ENOTDIR:
        return "Not a directory";
    case -ENODEV:
        return "No such device";
    case -ENOENT:
        return "No such file or directory";
    case -EINVAL:
        return "Invalid argument";
    default:
        return "Unknown error";
    }
}
