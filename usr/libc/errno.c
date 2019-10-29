#include <errno.h>
#include <stdio.h>

static const char *err_strings0[35] = {
    "Success",
    "Operation not permitted",
    "No such file or directory",
    "No such process",
    "Interrupted system call",
    "I/O error",
    "No such device or address",
    "Argument list too long",
    "Exec format error",
    "Bad file number",
    "No child processes",
    "Try again",
    "Out of memory",
    "Permission denied",
    "Bad address",
    "Block device required",
    "Device or resource busy",
    "File exists",
    "Cross-device link",
    "No such device",
    "Not a directory",
    "Is a directory",
    "Invalid argument",
    "File table overflow",
    "Too many open files",
    "Not a typewriter",
    "Text file busy",
    "File too large",
    "No space left on device",
    "Illegal seek",
    "Read-only file system",
    "Too many links",
    "Broken pipe",
    "Math argument out of domain of func",
    "Math result not representable"
};

int errno;

char *strerror(int n) {
    if (n >= 0 && n <= 34) {
        return (char *) err_strings0[n];
    }
    return (char *) "Unknown error";
}

void perror(const char *e) {
    if (e && *e) {
        printf("%s: %s\n", e, strerror(errno));
    } else {
        puts(strerror(errno));
    }
}
