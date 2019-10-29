#pragma once
#include <sys/errno.h>

extern int errno;

char *strerror(int errnum);
void perror(const char *s);
