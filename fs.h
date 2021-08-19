#ifndef _FS_H
#define _FS_H

#include <limits.h>
#define FLY_PATH_MAX	_POSIX_PATH_MAX	

struct fly_fs{
	char mount_path[FLY_PATH_MAX];
};

typedef struct fly_fs fly_fs_t;
extern fly_fs_t init_mount;

int fly_fs_mount(const char *path);

#endif 
