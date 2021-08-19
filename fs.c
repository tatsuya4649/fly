
#include <string.h>
#include "fs.h"

fly_fd_t init_mount;
int fly_fs_mount(const char *path)
{
	if (strlen(path) > FLY_PATH_MAX)
		return -1;
	strcpy(init_mount, path);
	init_mount[strlen(path)] = '\0';
	return 0;
}
