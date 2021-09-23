#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <limits.h>
#include <sys/stat.h>
#include <sys/sendfile.h>
#include <errno.h>
#include "mount.h"
#include "alloc.h"
#include "err.h"
#include "cache.h"
#include "response.h"

__fly_static int __fly_parts_file_remove(fly_mount_parts_t *parts, struct fly_mount_parts_file *pf);
__fly_static void __fly_path_cpy_with_mp(char *dist, char *src, const char *mount_point);
__fly_static int __fly_send_from_pf_blocking(fly_event_t *e, fly_response_t *response);
__fly_static int __fly_send_from_pf_blocking(fly_event_t *e, fly_response_t *response);
int fly_send_from_pf(fly_event_t *e, int c_sockfd, struct fly_mount_parts_file *pf, off_t *offset, size_t count);

int fly_mount_init(fly_context_t *ctx)
{
	fly_pool_t *pool;
	fly_mount_t *mnt;

	if (!ctx || !ctx->pool)
		return -1;
	pool = ctx->pool;

	mnt = fly_pballoc(pool, sizeof(fly_mount_t));
	if (mnt == NULL)
		return -1;

	mnt->mount_count = 0;
	mnt->ctx = ctx;
	mnt->parts = NULL;
	ctx->mount = mnt;

	return 0;
}

int fly_isdir(const char *path)
{
	struct stat stbuf;
	if (stat(path, &stbuf) == -1)
		return -1;
	return S_ISDIR(stbuf.st_mode);
}

int fly_isfile(const char *path)
{
	struct stat stbuf;
	if (stat(path, &stbuf) == -1)
		return -1;
	return S_ISREG(stbuf.st_mode);
}

ssize_t fly_file_size(const char *path)
{
	if (fly_isfile(path) <= 0)
		return -1;

	struct stat stbuf;
	if (stat(path, &stbuf) == -1)
		return -1;
	return (size_t) stbuf.st_size;
}

__fly_static void __fly_mount_path_cpy(char *dist, const char *src)
{
	int i=0;
	while(*src && i++<FLY_PATH_MAX)
		*dist++ = *src++;
	*dist = '\0';
	return;
}

__fly_static int __fly_mount_add(fly_mount_t *mnt, fly_mount_parts_t *parts)
{
	if (mnt->mount_count == 0){
		mnt->parts = parts;
	}else{
		fly_mount_parts_t *__p;
		for (__p=mnt->parts; __p->next; __p=__p->next){
			/* if same mount point, ignore */
			if (strcmp(__p->mount_path, parts->mount_path) == 0)
				return -1;
		}
		__p->next = parts;
	}
	parts->next = NULL;
	mnt->mount_count++;
	return 0;
}

void fly_parts_file_add(fly_mount_parts_t *parts, struct fly_mount_parts_file *pf)
{
	if (parts->file_count == 0){
		parts->files = pf;
		pf->next = NULL;
	}else{
		struct fly_mount_parts_file *__p;
		for (__p=parts->files; __p->next; __p=__p->next)
			;
		__p->next = pf;
	}
	parts->file_count++;
	return;
}

int fly_parts_file_remove(fly_mount_parts_t *parts, char *filename)
{
	if (parts->file_count == 0)
		return -1;

	for (struct fly_mount_parts_file *__pf=parts->files; __pf; __pf=__pf->next){
		if (strcmp(__pf->filename, filename) == 0)
			return __fly_parts_file_remove(parts, __pf);
	}
	return -1;
}

__fly_static int __fly_parts_file_remove(fly_mount_parts_t *parts, struct fly_mount_parts_file *pf)
{
	if (parts->file_count == 0)
		return -1;

	struct fly_mount_parts_file *prev = NULL;
	for (struct fly_mount_parts_file *__pf=parts->files; __pf; __pf=__pf->next){
		if (__pf == pf){
			if (prev == NULL)
				parts->files = __pf->next;
			else
				prev->next = __pf->next;
			/* release __pf */
			fly_pbfree(parts->mount->ctx->pool, __pf);
			parts->file_count--;
			return 0;
		}

		prev = __pf;
	}

	/* not found */
	return -1;
}

__fly_static void __fly_path_cpy_with_mp(char *dist, char *src, const char *mount_point)
{
	/* ignore up to mount point */
	while (*src++ == *mount_point++)
		;

	if (*src == '/')	src++;
	while (*src)
		*dist++ = *src++;
}

__fly_static int __fly_nftw(fly_mount_parts_t *parts, const char *path, const char *mount_point, int infd)
{
	DIR *__pathd;
	struct fly_mount_parts_file *pfile;
	struct dirent *__ent;
	struct stat sb;
	char __path[FLY_PATH_MAX];
	fly_pool_t *pool;
	int res;

	pool = parts->mount->ctx->pool;

	__pathd = opendir(path);
	if (__pathd == NULL)
		return -1;

	while((__ent=readdir(__pathd)) != NULL){
		if (strcmp(__ent->d_name, ".") == 0 || \
				strcmp(__ent->d_name, "..") == 0)
			continue;

		res = snprintf(__path, FLY_PATH_MAX, "%s/%s", path, __ent->d_name);
		if (res < 0 || res == FLY_PATH_MAX)
			continue;
		if (strlen(__ent->d_name) >= FLY_PATHNAME_MAX)
			continue;

		if (stat(__path, &sb) == -1)
			continue;
		/* recursion */
		if (S_ISDIR(sb.st_mode))
			if (__fly_nftw(parts, __path, mount_point, infd) == -1)
				goto error;

		pfile = fly_pballoc(pool, sizeof(struct fly_mount_parts_file));
		if (fly_unlikely_null(pfile))
			goto error;
		pfile->fd = -1;
		memset(pfile->filename, 0, FLY_PATHNAME_MAX);
		__fly_path_cpy_with_mp(pfile->filename, __path, mount_point);
		pfile->parts = parts;
		pfile->next = NULL;
		pfile->infd = parts->infd;
		pfile->mime_type = fly_mime_type_from_path_name(__path);
		if (infd >= 0){
			if (strcmp(path, mount_point) == 0)
				pfile->wd = inotify_add_watch(infd, __path, FLY_INOTIFY_WATCH_FLAG_MP);
			else
				pfile->wd = inotify_add_watch(infd, __path, FLY_INOTIFY_WATCH_FLAG_PF);
			if (pfile->wd == -1)
				goto error;
		}else
			pfile->wd = -1;
		if (fly_hash_from_parts_file_path(__path, pfile) == -1)
			goto error;

		fly_parts_file_add(parts, pfile);
	}

	return closedir(__pathd);
error:
	closedir(__pathd);
	return -1;
}

int fly_mount(fly_context_t *ctx, const char *path)
{
	fly_mount_t *mnt;
	fly_mount_parts_t *parts;
	fly_pool_t *pool;
	char rpath[FLY_PATH_MAX];

	if (!ctx || !ctx->mount)
		return -1;
	if (realpath(path, rpath) == NULL)
		return -1;
	mnt = ctx->mount;

	if (path == NULL || strlen(path) > FLY_PATH_MAX)
		return FLY_EARG;
	if (fly_isdir(rpath) != 1)
		return FLY_EARG;

	pool = mnt->ctx->pool;
	parts = fly_pballoc(pool, sizeof(fly_mount_parts_t));
	if (parts == NULL)
		return -1;

	__fly_mount_path_cpy(parts->mount_path, rpath);
	parts->mount_number = mnt->mount_count;
	parts->mount = mnt;
	parts->next = NULL;
	parts->wd = -1;
	parts->infd = -1;
	parts->pool = pool;
	parts->file_count = 0;

	if (__fly_mount_add(mnt, parts) == -1)
		goto error;
	if (__fly_nftw(parts, rpath, rpath, -1) == -1)
		goto error;

	return 0;
error:
	fly_pbfree(pool, parts);
	return -1;
}

int fly_unmount(fly_mount_t *mnt, const char *path)
{
	if (mnt->mount_count == 0){
		return 0;
	}else{
		fly_mount_parts_t *__p, *prev = NULL;
		for (__p=mnt->parts; __p; __p=__p->next){
			/* if same mount point, ignore */
			if (strcmp(__p->mount_path, path) == 0){
				if (prev)
					prev->next = __p->next;
				else
					mnt->parts = __p->next;
				/* release parts */
				fly_pbfree(__p->pool, __p);
				mnt->mount_count--;
				goto check_total_mount_count;
			}

			prev = __p;
		}
	}
	/* not found */
	return 0;
check_total_mount_count:
	/* no mount point */
	if (mnt->mount_count == 0)
		/* emergency error. log and end process. */
		FLY_EMERGENCY_ERROR(
			FLY_EMERGENCY_STATUS_NOMOUNT,
			"There is no mount point."
		);
	return 0;
}

int fly_mount_number(fly_mount_t *mnt, const char *path)
{
	if (!mnt || !mnt->mount_count)
		return FLY_EARG;

	for (fly_mount_parts_t *__p=mnt->parts; __p; __p=__p->next){
		if (strcmp(__p->mount_path, path) == 0)
			return __p->mount_number;
	}
	return FLY_ENOTFOUND;
}

int fly_join_path(char *buffer, char *join1, char *join2)
{
	char *ptr;
	char result[FLY_PATH_MAX];
	if (strlen(join1)+strlen(join2)+1 >= FLY_PATH_MAX)
		return -1;
	ptr = result;
	strcpy(ptr, join1);
	ptr += strlen(join1);
	strcpy(ptr, "/");
	ptr += 1;
	strcpy(ptr, join2);
	ptr += strlen(join2);
	strcpy(ptr, "\0");

	if (realpath(result, buffer) == NULL)
		return -1;

	return 0;
}

/*
 * if there are same name file on some mount points,
 * send file that found first.
 */
//int fly_from_path(int c_sockfd, fly_mount_t *mnt, int mount_number, char *filename, off_t *offset, size_t count)
//{
//	for (fly_mount_parts_t *__p=mnt->parts; __p; __p=__p->next){
//		/* no file, this mount point */
//		if (__p->file_count == 0)
//			continue;
//		if (mount_number < 0 || mount_number != __p->mount_number)
//			continue;
//
//		for (struct fly_mount_parts_file *__pf; __pf; __pf=__pf->next){
//			if (strcmp(filename, __pf->filename) == 0){
//				if (__fly_send_file(c_sockfd, __pf, offset, count) == -1)
//					return -1;
//				else
//					return 0;
//			}
//		}
//	}
//
//	/* TODO: not found(404) */
//	return -1;
//}

__fly_static int __fly_send_from_pf_blocking_handler(fly_event_t *e)
{
	fly_response_t *res;

	res = (fly_response_t *) e->event_data;
	return fly_send_from_pf(e, e->fd, res->pf, &res->offset, res->count);
}

__fly_static int __fly_send_from_pf_blocking(fly_event_t *e, fly_response_t *response)
{
	e->event_data = (void *) response;
	e->read_or_write = FLY_WRITE;
	e->eflag = 0;
	e->tflag = FLY_INHERIT;
	e->flag = FLY_NODELETE;
	e->available = false;
	FLY_EVENT_HANDLER(e, __fly_send_from_pf_blocking_handler);
	return fly_event_register(e);
}

int fly_send_from_pf(fly_event_t *e, int c_sockfd, struct fly_mount_parts_file *pf, off_t *offset, size_t count)
{
	fly_response_t *response;
	size_t total;
	int *bfs;
	ssize_t numsend;

	response = (fly_response_t *) e->event_data;
	bfs = &response->byte_from_start;
	response = (fly_response_t *) e->event_data;
	response->fase = FLY_RESPONSE_BODY;
	response->pf = pf;
	response->offset = *offset;
	response->count = count;

	if (*bfs)
		total = (size_t) *bfs;
	else
		total = 0;

	*offset += total;
	while(total < count){
		numsend = sendfile(c_sockfd, pf->fd, offset, count-total);
		if (FLY_BLOCKING(numsend)){
			/* event register */
			if (__fly_send_from_pf_blocking(e, response) == -1)
				return FLY_RESPONSE_ERROR;
			return FLY_RESPONSE_BLOCKING;
		}else if (numsend == -1)
			return FLY_RESPONSE_ERROR;

		total += numsend;
		*bfs = total;
	}

	*bfs = 0;
	response->fase = FLY_RESPONSE_RELEASE;
	return FLY_RESPONSE_SUCCESS;
}

#define FLY_FOUND_CONTENT_FROM_PATH_FOUND		1
#define FLY_FOUND_CONTENT_FROM_PATH_NOTFOUND	0
#define FLY_FOUND_CONTENT_FROM_PATH_ERROR		-1
__fly_static int __fly_uri_matching(char *filename, fly_uri_t *uri)
{
	size_t i=0;
	char *uri_str;
	if (*uri->ptr == '/')
		i++;

	if (uri->ptr[i] == '\0' || i>=uri->len)
		uri_str = FLY_URI_INDEX_NAME;
	else
		uri_str = uri->ptr;

	while(filename[i] == uri_str[i]){
		if ((*filename == '\0' && *uri_str == '\0') || \
				i>=uri->len)
			return 0;
		i++;
	}

	return -1;
}

int fly_found_content_from_path(fly_mount_t *mnt, fly_uri_t *uri, struct fly_mount_parts_file **res)
{
	for (fly_mount_parts_t *__p=mnt->parts; __p; __p=__p->next){
		if (fly_unlikely(__p->file_count == 0))
			continue;

		for (struct fly_mount_parts_file *__pf=__p->files; __pf; __pf=__pf->next){
			if (__fly_uri_matching(__pf->filename, uri) == 0){
				*res = __pf;
				return FLY_FOUND_CONTENT_FROM_PATH_FOUND;
			}
		}
	}

	*res = NULL;
	return FLY_FOUND_CONTENT_FROM_PATH_NOTFOUND;
}

int fly_mount_inotify(fly_mount_t *mount, int ifd)
{
	for (int i=0; i<mount->mount_count; i++){
		fly_mount_parts_t *parts;
		parts = &mount->parts[i];
		/* inotify add watch dir */
		int dwd;
		dwd = inotify_add_watch(ifd, parts->mount_path, FLY_INOTIFY_WATCH_FLAG_MP);
		if (dwd == -1)
			return -1;
		parts->wd = dwd;
		parts->infd = ifd;

		if (parts->file_count == 0)
			continue;
		/* inotify add watch file */
		for (struct fly_mount_parts_file *__pf=parts->files; __pf; __pf=__pf->next){
			int wd;
			char rpath[FLY_PATH_MAX];

			if (fly_join_path(rpath, parts->mount_path, __pf->filename) == -1)
				continue;
			wd = inotify_add_watch(ifd, rpath, FLY_INOTIFY_WATCH_FLAG_PF);
			if (wd == -1)
				return -1;

			__pf->wd = wd;
			__pf->infd = ifd;
		}
	}
	return 0;
}

struct fly_mount_parts_file *fly_pf_from_parts(char *path, fly_mount_parts_t *parts)
{
	char __path[FLY_PATH_MAX];
	int res;
	if (parts->file_count == 0)
		return NULL;

	for (struct fly_mount_parts_file *__pf=parts->files; __pf; __pf=__pf->next){
		res = snprintf(__path, FLY_PATH_MAX, "%s/%s", parts->mount_path, __pf->filename);
		if (res < 0 || res >= FLY_PATH_MAX)
			return NULL;
		if (strcmp(path, __path) == 0)
			return __pf;
	}
	return NULL;
}

struct fly_mount_parts_file *fly_wd_from_pf(int wd, fly_mount_parts_t *parts)
{
	if (parts->file_count == 0)
		return NULL;

	for (struct fly_mount_parts_file *__pf=parts->files; __pf; __pf=__pf->next){
		if (__pf->wd == wd)
			return __pf;
	}
	return NULL;
}

struct fly_mount_parts_file *fly_wd_from_mount(int wd, fly_mount_t *mnt)
{
	struct fly_mount_parts_file *pf;
	if (mnt->mount_count == 0)
		return NULL;

	for (fly_mount_parts_t *__p=mnt->parts; __p; __p=__p->next){
		pf = fly_wd_from_pf(wd, __p);
		if (pf)
			return pf;
	}
	return NULL;
}

fly_mount_parts_t *fly_wd_from_parts(int wd, fly_mount_t *mnt)
{
	if (mnt->mount_count == 0)
		return NULL;

	for (fly_mount_parts_t *__p=mnt->parts; __p; __p=__p->next){
		if (__p->wd == wd)
			return __p;
	}
	return NULL;
}

int fly_inotify_add_watch(fly_mount_parts_t *parts, char *path)
{
	struct fly_mount_parts_file *__npf;
	char rpath[FLY_PATH_MAX];

	if (fly_join_path(rpath, parts->mount_path, path) == -1)
		return -1;

	if (fly_isdir(rpath))
		if (__fly_nftw(parts, (const char *) rpath, parts->mount_path, parts->infd) == -1)
			return -1;

	__npf = fly_pballoc(parts->mount->ctx->pool, sizeof(struct fly_mount_parts_file));
	if (fly_unlikely_null(__npf))
		return -1;

	__npf->infd = parts->infd;
	__npf->fd = -1;
	__npf->wd = inotify_add_watch(__npf->infd, rpath, FLY_INOTIFY_WATCH_FLAG_PF);
	__npf->parts = parts;
	strcpy(__npf->filename, path);
	if (fly_hash_from_parts_file_path(rpath, __npf) == -1)
		return -1;
	fly_parts_file_add(parts, __npf);
	return 0;
}

__fly_static int __fly_samedir_cmp(char *s1, char *s2)
{
	bool slash = false;
	while(*s1 == *s2 && *s1 != '\0'){
		if (*s1 == '/')
			slash = true;
		s1++;
		s2++;
	}
	if ((slash && *s1 == '\0') || *s1 == '/' || (*s1 == '\0' && *s2 == '\0'))
		return 0;
	return -1;
}

int fly_inotify_rmmp(fly_mount_parts_t *parts)
{
	/* remove mount point */
	for (struct fly_mount_parts_file *__pf=parts->files; __pf;__pf=__pf->next){
		if(inotify_rm_watch(__pf->infd, __pf->wd) == -1)
			return -1;
		if (__fly_parts_file_remove(parts, __pf) == -1)
			return -1;
	}

	struct stat statb;
	if (stat(parts->mount_path, &statb) == 0 && \
			inotify_rm_watch(parts->infd, parts->wd) == -1)
		return -1;

	if (fly_unmount(parts->mount, parts->mount_path) == -1)
		return -1;
	return 0;
}

int fly_inotify_rm_watch(fly_mount_parts_t *parts, char *path, int mask)
{
	/* remove mount point elements */
	if (parts->file_count == 0)
		return -1;
	for (struct fly_mount_parts_file *__pf=parts->files; __pf; __pf=__pf->next){
		if (__fly_samedir_cmp(__pf->filename, path) == 0){
			if (mask & IN_MOVED_FROM && \
					inotify_rm_watch(__pf->infd, __pf->wd) == -1)
				return -1;
			if (__fly_parts_file_remove(parts, __pf) == -1)
				return -1;
		}
	}

	return 0;
}
