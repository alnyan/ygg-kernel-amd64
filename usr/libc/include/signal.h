#pragma once
#include <sys/signum.h>

typedef void (*sighandler_t) (int);

sighandler_t signal(int signum, sighandler_t handler);

int raise(int signum);
