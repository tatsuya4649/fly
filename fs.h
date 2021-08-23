#ifndef _FS_H
#define _FS_H

#include "alloc.h"
#include <limits.h>
#define FLY_PATH_MAX	_POSIX_PATH_MAX	
#define FLY_FS_POOL_PAGE		((fly_page_t) 10)
#define FLY_FS_INIT_NUMBER		0

struct fly_fs{
	char mount_path[FLY_PATH_MAX];
	int mount_number;
	struct fly_fs *next;
};
#ifdef FLY_TEST
extern fly_pool_t *fspool;
#endif
typedef struct fly_fs fly_fs_t;
extern fly_fs_t *init_mount;

int fly_fs_init(void);
int fly_fs_release(void);
int fly_fs_mount(const char *path);


int fly_fs_isdir(const char *path);
int fly_fs_isfile(const char *path);
ssize_t fly_file_size(const char *path);
int fly_mount_number(const char *path);
char *fly_content_from_path(int mount_number, char *filepath);
int fly_join_path(char *buffer, char *join1, char *join2);
void *fly_from_path(fly_pool_t *pool, fly_pool_s size, int mount_number, char *filepath);

#endif 
