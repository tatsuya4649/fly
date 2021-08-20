#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <limits.h>
#include <sys/stat.h>
#include <errno.h>
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

int fly_fs_isfile(const char *path)
{
	struct stat stbuf;
	if (stat(path, &stbuf) == -1)
		return -1;
	return S_ISREG(stbuf.st_mode);
}

ssize_t fly_file_size(const char *path)
{
	if (fly_fs_isfile(path) <= 0)
		return -1;
	
	struct stat stbuf;
	if (stat(path, &stbuf) == -1)
		return -1;
	return (size_t) stbuf.st_size;
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

int fly_mount_number(const char *path)
{
	fly_fs_t *now;
	for (now=init_mount; now->next!=NULL; now=now->next){
		if (strcmp(now->mount_path, path) == 0)
			return now->mount_number;
	}
	return -1;
}

int fly_join_path(char *buffer, char *join1, char *join2)
{
	char *ptr;
	if (strlen(join1)+strlen(join2)+1 >= FLY_PATH_MAX)
		return -1;
	ptr = buffer;
	strcpy(ptr, join1);
	ptr += strlen(join1);
	strcpy(ptr, "/");
	ptr += 1;
	strcpy(ptr, join2);
	ptr += strlen(join2);
	strcpy(ptr, "\0");
	return 0; 
}

void *fly_memory_from_size(fly_pool_t *pool, fly_pool_s size)
{
	for (struct fly_size_bytes *sbyte=fly_sizes; sbyte->size>=0; sbyte++){
		if (size == sbyte->size)
			return fly_pballoc(pool, FLY_KB*sbyte->kb);
	}
	return NULL;
}

void *fly_from_path(fly_pool_t *pool, fly_pool_s size, int mount_number, char *filepath)
{
	fly_fs_t *fs;
	char join_path[FLY_PATH_MAX];
	FILE *fp;
	ssize_t fp_size;
	ssize_t size_bytes;
	for (fs=init_mount; fs!=NULL; fs=fs->next){
		if (fs->mount_number == mount_number){
			if (fly_join_path(join_path, fs->mount_path, filepath) == -1)
				return NULL;
			if (fly_fs_isfile(join_path)<=0)
				return NULL;

			fp = fopen(join_path, "rb");
			if (fp == NULL)
				return NULL;
			void *start_ptr = fly_memory_from_size(pool, size);
			fp_size = fly_file_size(join_path);
			if (fp_size < 0)
				return NULL;
			size_bytes = fly_bytes_from_size(size);
			if (size_bytes < -1)
				return NULL;
			if (fp_size > size_bytes)
				return NULL;

			ssize_t readsize;
			while ((readsize = fread(start_ptr, sizeof(char), fp_size, fp)) != 0){
				if (readsize == -1){
					if (errno == EINTR)
						continue;
					else
						return NULL;
				}
			}
			return start_ptr;
		}
	}
	return NULL;
}
