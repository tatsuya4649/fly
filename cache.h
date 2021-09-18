#ifndef _CACHE_H
#define _CACHE_

#include <time.h>
#include <openssl/md5.h>

struct fly_mount_parts_file;

#define FLY_MD5_LENGTH		(16)
struct fly_file_hash{
	time_t mtime;
	time_t ctime;

	unsigned char md5[(2*FLY_MD5_LENGTH)+1];

	struct fly_mount_parts_file *pf;
};

int fly_hash_from_parts_file(struct fly_mount_parts_file *pf);
int fly_hash_from_parts_file_path(char *path, struct fly_mount_parts_file *pf);
int fly_hash_update_from_parts_file(struct fly_mount_parts_file *pf);
int fly_hash_update_from_parts_file_path(char *path, struct fly_mount_parts_file *pf);

#endif
