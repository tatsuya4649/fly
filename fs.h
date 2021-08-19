#ifndef _FS_H
#define _FS_H

#include <limits.h>
#define FLY_PATH_MAX	_POSIX_PATH_MAX	
#define FLY_FS_POOL_PAGE		((fly_page_t) 10)

struct fly_fs{
	char mount_path[FLY_PATH_MAX];
	int mount_number;
	struct fly_fs *next;
};

typedef struct fly_fs fly_fs_t;
extern fly_fs_t *init_mount;

int fly_fs_init(void);
int fly_fs_release(void);
int fly_fs_mount(const char *path);

#endif 
