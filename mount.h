#ifndef _MOUNT_H
#define _MOUNT_H

#include "alloc.h"
#include <dirent.h>
#include <limits.h>
#include <sys/inotify.h>
#define FLY_PATHNAME_MAX	_POSIX_NAME_MAX
#define FLY_PATH_MAX	_POSIX_PATH_MAX
#define FLY_MOUNT_POOL_PAGE		((fly_page_t) 10)
#define FLY_MOUNT_INIT_NUMBER		0

struct fly_mount_parts_file{
	int fd;
	int wd;
	char filename[FLY_PATHNAME_MAX];
	struct fly_mount_parts *parts;
	struct fly_file_hash *hash;

	struct fly_mount_parts_file *next;
};

struct fly_mount_parts{
	int wd;
	char mount_path[FLY_PATH_MAX];
	int mount_number;

	struct fly_mount_parts_file *files;
	int file_count;

	struct fly_mount_parts *next;
	struct fly_mount *mount;
};

struct fly_context;
typedef struct fly_context fly_context_t;
struct fly_mount{
	struct fly_mount_parts *parts;
	int mount_count;

	fly_context_t *ctx;
};
typedef struct fly_mount fly_mount_t;
typedef struct fly_mount_parts fly_mount_parts_t;

int fly_mount_init(fly_context_t *ctx);
int fly_mount(fly_context_t *ctx, const char *path);
int fly_unmount(fly_mount_t *mnt, const char *path);

int fly_isdir(const char *path);
int fly_isfile(const char *path);
ssize_t fly_file_size(const char *path);
int fly_mount_number(fly_mount_t *mnt, const char *path);
char *fly_content_from_path(int mount_number, char *filepath);
int fly_join_path(char *buffer, char *join1, char *join2);
int fly_from_path(int c_sockfd, fly_mount_t *mnt, int mount_number, char *filename, off_t *offset, size_t count);

int fly_mount_inotify(fly_mount_t *mount, int ifd);

#endif
