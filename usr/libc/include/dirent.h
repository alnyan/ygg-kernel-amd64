#pragma once
#include <sys/fs/dirent.h>

typedef struct DIR_private DIR;

DIR *opendir(const char *path);
void closedir(DIR *dirp);
struct dirent *readdir(DIR *dirp);
