#ifndef _CACHE_H
#define _CACHE_H

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

#include "header.h"
#define FLY_IMT_FIXDATE_FORMAT			("%a, %d %b %Y %H:%M:%S GMT")
#define FLY_IF_NONE_MATCH				("If-None-Match")
#define FLY_IF_MODIFIED_SINCE			("If-Modified-Since")
int fly_if_none_match(fly_hdr_ci *ci, struct fly_mount_parts_file *pf);
int fly_if_modified_since(fly_hdr_ci *ci, struct fly_mount_parts_file *pf);


#endif
