#include "sys/errno.h"

const char *kstrerror(int e) {
    switch (e) {
    case -ENOENT:
        return "No such file or directory";
    case -EINVAL:
        return "Invalid argument";
    default:
        return "Unknown error";
    }
}
