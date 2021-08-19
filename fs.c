#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <sys/stat.h>
#include "fs.h"
#include "alloc.h"

fly_fs_t *init_mount;
fly_pool_t *fspool;

int fly_fs_init(void)
{
	fspool = fly_create_pool(FLY_FS_POOL_PAGE);
	if (fspool == NULL)
		return -1;
	init_mount = NULL;
	return 0;
}

int fly_fs_isdir(const char *path)
{
	struct stat stbuf;
	if (stat(path, &stbuf) == -1)
		return -1;
	return S_ISDIR(stbuf.st_mode);
}

int fly_fs_mount(const char *path)
{
	fly_fs_t *now;
	if (strlen(path) > FLY_PATH_MAX)
		return -1;

	if (init_mount == NULL){
		init_mount = fly_pballoc(fspool, sizeof(fly_fs_t));
		if (init_mount == NULL)
			return -1;
		if (realpath(path, init_mount->mount_path) == NULL)
			return -1;
		init_mount->mount_path[strlen(path)] = '\0';
		if (fly_fs_isdir(init_mount->mount_path) == -1)
			return -1;
	}else{
		for (now=init_mount; now->next!=NULL; now=now->next){
			if (strcmp(now->mount_path, path) == 0)
				return 0;
		}

		fly_fs_t *newfs;
		newfs = fly_pballoc(fspool, sizeof(fly_fs_t));
		if (newfs == NULL)
			return -1;

		if (realpath(path, newfs->mount_path) == NULL)
			return -1;
		if (fly_fs_isdir(newfs->mount_path) == -1)
			return -1;
		newfs->mount_number += now->mount_number+1;
		newfs->next = NULL;
		now->next = newfs;
	}
	return 0;
}

int fly_fs_release()
{
	fly_delete_pool(fspool);
	return 0;
}
