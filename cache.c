#include "cache.h"
#include "mount.h"
#include "util.h"
#include "context.h"
#include <sys/stat.h>
#include <stdio.h>

__fly_static int __fly_number_of_digits_time(time_t __t)
{
	int i=0;
	time_t a;

	for (a=__t; a; a/=10)
		i++;

	return i;
}

__fly_static int __fly_md5_from_hash(struct fly_file_hash *hash)
{
	MD5_CTX c;
	char *md5_src;
	char __preload[FLY_MD5_LENGTH+1];
	int mtime_len, ctime_len, res, md5_len;

	mtime_len = __fly_number_of_digits_time(hash->mtime);
	ctime_len = __fly_number_of_digits_time(hash->ctime);

	md5_len = strlen(hash->pf->filename) + mtime_len + ctime_len + 1;

	md5_src = fly_pballoc(hash->pf->parts->mount->ctx->pool, sizeof(char)*(md5_len));

	res = snprintf(md5_src, md5_len, "%s%ld%ld", hash->pf->filename, hash->mtime, hash->ctime);
	if (res < 0 || res >= md5_len)
		return -1;

	if (MD5_Init(&c) == -1)
		return -1;

	if (MD5_Update(&c, md5_src, strlen(md5_src)) == -1)
		return -1;

	if (MD5_Final((unsigned char *) __preload, &c) == -1)
		return -1;

	for (int i=0; i<FLY_MD5_LENGTH; i++){
		if (snprintf((char *) &hash->md5[2*i], 3, "%02x", __preload[i]) == -1)
			return -1;
	}
	hash->md5[2*FLY_MD5_LENGTH+1] = '\0';

	/* TODO: release md5_src */
	return 0;
}

__fly_static int __fly_hash_from_parts_file(struct stat *statbuf, struct fly_mount_parts_file *pf)
{
	struct fly_file_hash *hash;

	hash = fly_pballoc(pf->parts->mount->ctx->pool, sizeof(struct fly_file_hash));
	if (fly_unlikely_null(hash))
		return -1;

	hash->mtime = statbuf->st_mtime;
	hash->ctime = statbuf->st_ctime;
	hash->pf = pf;
	if (__fly_md5_from_hash(hash) == -1)
		return -1;

	pf->hash = hash;
	return 0;
}

int fly_hash_from_parts_file_path(char *path, struct fly_mount_parts_file *pf)
{
	struct stat statbuf;
	if (stat(path, &statbuf) == -1)
		return -1;
//	if (fly_unlikely(!S_ISREG(statbuf.st_mode)))
//		return -1;

	return __fly_hash_from_parts_file(&statbuf, pf);
}

int fly_hash_from_parts_file(struct fly_mount_parts_file *pf)
{
	struct stat statbuf;

	if (fstat(pf->fd, &statbuf) == -1)
		return -1;

	if (fly_unlikely(!S_ISREG(statbuf.st_mode)))
		return -1;

	return __fly_hash_from_parts_file(&statbuf, pf);
}

__fly_static int __fly_hash_update(struct stat *statbuf, struct fly_mount_parts_file *pf)
{
	if (fly_unlikely_null(pf->hash))
		return -1;

	pf->hash->mtime = statbuf->st_mtime;
	pf->hash->ctime = statbuf->st_ctime;
	pf->hash->pf = pf;
	if (__fly_md5_from_hash(pf->hash) == -1)
		return -1;
	return 0;
}

int fly_hash_update_from_parts_file(struct fly_mount_parts_file *pf)
{
	struct stat statbuf;
	if (fstat(pf->fd, &statbuf) == -1)
		return -1;

	return __fly_hash_update(&statbuf, pf);
}
int fly_hash_update_from_parts_file_path(char *path, struct fly_mount_parts_file *pf)
{
	struct stat statbuf;
	if (stat(path, &statbuf) == -1)
		return -1;
	return __fly_hash_update(&statbuf, pf);
}
