#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <limits.h>
#include <sys/stat.h>
#include <errno.h>
#include "mount.h"
#include "alloc.h"
#include "err.h"
#include "cache.h"
#include "response.h"
#include "rbtree.h"

__fly_static void __fly_path_cpy_with_mp(char *dist, char *src, const char *mount_point, size_t dist_len);
static int fly_mount_max_limit(void);
static size_t fly_file_max_limit(void);
static int __fly_mount_search_cmp(fly_rbdata_t *k1, fly_rbdata_t *k2, fly_rbdata_t *cmpdata);
#ifdef HAVE_KQUEUE
static int fly_parts_set_newdir(fly_mount_parts_t *parts, DIR *dir);
#endif

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
	mnt->file_count = 0;
	mnt->ctx = ctx;
	mnt->index = NULL;
	fly_bllist_init(&mnt->parts);
	mnt->rbtree = fly_rb_tree_init(__fly_mount_search_cmp);
	ctx->mount = mnt;

	return 0;
}

void fly_mount_release(fly_context_t *ctx)
{
	if (ctx == NULL || ctx->mount == NULL)
		return;
	fly_rb_tree_release(ctx->mount->rbtree);
	fly_pbfree(ctx->pool, ctx->mount);
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
	fly_bllist_add_tail(&mnt->parts, &parts->mbelem);
	mnt->mount_count++;
	return 0;
}

void fly_parts_file_add(fly_mount_parts_t *parts, struct fly_mount_parts_file *pf)
{
	fly_bllist_add_tail(&parts->files, &pf->blelem);
	parts->file_count++;
	parts->mount->file_count++;
	return;
}

int fly_parts_file_remove_from_path(fly_mount_parts_t *parts, char *filename)
{
	if (parts->file_count == 0)
		return -1;

	struct fly_bllist *__b;
	fly_for_each_bllist(__b, &parts->files){
		struct fly_mount_parts_file *__pf = fly_bllist_data(__b, struct fly_mount_parts_file, blelem);
		if (strcmp(__pf->filename, filename) == 0){
			fly_parts_file_remove(parts, __pf);
			return 0;
		}
	}
	return -1;
}

bool fly_pf_from_parts_with_path(fly_mount_parts_t *parts, char *fullpath, size_t pathlen)
{
	if (parts->file_count == 0)
		return false;

	int res;
	struct fly_bllist *__b;
	char rpath[FLY_PATH_MAX];
	fly_for_each_bllist(__b, &parts->files){
		struct fly_mount_parts_file *__pf = fly_bllist_data(__b, struct fly_mount_parts_file, blelem);

		res = snprintf(rpath, FLY_PATH_MAX, "%s/%s", parts->mount_path, __pf->filename);
		if (res < 0 || res == FLY_PATH_MAX)
			continue;
		if (pathlen != (size_t) res)
			continue;

		if (strncmp(fullpath, rpath, pathlen) == 0)
			return true;
	}
	return false;
}


void fly_parts_file_remove(fly_mount_parts_t *parts, struct fly_mount_parts_file *pf)
{
	fly_bllist_remove(&pf->blelem);
	if (pf->rbnode != NULL)
		fly_rb_delete(parts->mount->rbtree, pf->rbnode);
	fly_pbfree(parts->mount->ctx->pool, pf);
	parts->file_count--;
	parts->mount->file_count--;
}

__fly_static void __fly_path_cpy_with_mp(char *dist, char *src, const char *mount_point, size_t dist_len)
{
	char *__d=dist;
	/* ignore up to mount point */
	while (*src++ == *mount_point++)
		;

	if (*src == '/')	src++;
	while (*src)
		*dist++ = *src++;

	__d[dist_len-1] = '\0';
}

struct fly_mount_parts_file *fly_pf_init(fly_mount_parts_t *parts, struct stat *sb)
{
	fly_pool_t *pool;
	struct fly_mount_parts_file *pfile;

	pool = parts->mount->ctx->pool;
	pfile = fly_pballoc(pool, sizeof(struct fly_mount_parts_file));
	pfile->fd = -1;
#ifdef HAVE_INOTIFY
	pfile->wd = -1;
	pfile->infd = parts->infd;
#elif defined HAVE_KQUEUE
	pfile->event = NULL;
#endif
	memset(pfile->filename, '\0', FLY_PATHNAME_MAX);
	pfile->filename_len = 0;
	memcpy(&pfile->fs, sb, sizeof(struct stat));
	pfile->parts = parts;
	pfile->mime_type = NULL;
	pfile->de = NULL;
	pfile->encode_type = FLY_MOUNT_DEFAULT_ENCODE_TYPE;
	pfile->encoded = false;
	pfile->rbnode = NULL;
	pfile->overflow = false;
	if (S_ISDIR(sb->st_mode))
		pfile->dir = true;
	else
		pfile->dir = false;
	pfile->deleted = false;

	return pfile;
}

void fly_pf_release(struct fly_mount_parts_file *__pf)
{
#ifdef DEBUG
	assert(__pf != NULL);
	assert(__pf->parts != NULL);
	assert(__pf->parts->mount != NULL);
	assert(__pf->parts->mount->ctx != NULL);
	assert(__pf->parts->mount->ctx->pool != NULL);
#endif
	fly_pbfree(__pf->parts->mount->ctx->pool, __pf);
}

#ifdef DEBUG
static void __fly_mount_debug(struct fly_mount *mnt)
{
	struct fly_bllist *__b, *__pb;
	struct fly_mount_parts *__p;
	struct fly_mount_parts_file *__pf;

	printf("MOUNT DEBUG:\n");
	fly_for_each_bllist(__b, &mnt->parts)
	{
		__p = fly_bllist_data(__b, struct fly_mount_parts, mbelem);
		printf("\tMOUNT POINT[%d]: %s(file count: %d)\n", __p->mount_number, __p->mount_path, __p->file_count);

#ifdef HAVE_INOTIFY
		printf("\t\tmount point %s, wd %d\n", __p->mount_path, __p->wd);
#else
		printf("\t\tmount point %s, fd %d\n", __p->mount_path, __p->fd);
#endif
		fly_for_each_bllist(__pb, &__p->files)
		{
			__pf = fly_bllist_data(__pb, struct fly_mount_parts_file, blelem);
			printf("\t\t%s: %s, fd %d\n", __pf->dir ? "DIR ": "FILE", __pf->filename, __pf->fd);
		}
	}
}
#endif

#ifdef HAVE_INOTIFY
__fly_static int __fly_nftw(fly_mount_parts_t *parts, const char *path, const char *mount_point, int infd)
#else
__fly_static int __fly_nftw(fly_event_t *event, fly_mount_parts_t *parts, const char *path, const char *mount_point, int infd)
#endif
{
	DIR *__pathd;
	struct fly_mount_parts_file *pfile;
	struct dirent *__ent;
	struct stat sb;
	char __path[FLY_PATH_MAX];
	int res;

#ifdef DEBUG
	assert(parts->mount != NULL);
	assert(parts->mount->ctx != NULL);
#endif
	if (parts->mount->file_count > fly_file_max_limit())
		/* file resource error */
		return -1;
	__pathd = opendir(path);
	if (__pathd == NULL)
		return -1;

	while((__ent=readdir(__pathd)) != NULL){
		if (strcmp(__ent->d_name, ".") == 0 || \
				strcmp(__ent->d_name, "..") == 0)
			continue;

		res = snprintf(__path, FLY_PATH_MAX, "%s/%s", path, __ent->d_name);
		if (res < 0 || res == FLY_PATH_MAX){
			fly_log_t *__log;
#ifdef HAVE_INOTIFY
			__log = parts->mount->ctx->log;
#else
			if (event == NULL)
				continue;

			__log = fly_context_from_event(event)->log;
#endif
#ifdef DEBUG
			assert(__log != NULL);
#endif
			fly_notice_direct_log(
				__log,
				"can't mount \"%s\" because path name is too long. path name length must be %d characters or less.",
				__path, FLY_PATHNAME_MAX
			);
			continue;
		}
		if (strlen(__ent->d_name) >= FLY_PATHNAME_MAX){
			fly_log_t *__log;
#ifdef HAVE_INOTIFY
			__log = parts->mount->ctx->log;
#else
			if (event == NULL)
				continue;

			__log = fly_context_from_event(event)->log;
#endif
#ifdef DEBUG
			assert(__log != NULL);
#endif
			fly_notice_direct_log(
				__log,
				"can't mount \"%s\" because path name is too long. directory entry name length must be %d characters or less.",
				__path, FLY_PATHNAME_MAX
			);
			continue;
		}

		if (stat(__path, &sb) == -1)
			continue;
		/* recursion */
		if (S_ISDIR(sb.st_mode)){
#ifdef HAVE_INOTIFY
			if (__fly_nftw(parts, __path, mount_point, infd) == -1){
#elif defined HAVE_KQUEUE
			if (__fly_nftw(event, parts, __path, mount_point, infd) == -1){
#else
#error  not found inotify or kqueue on your system
#endif
				goto error;
			}
		}

		/* already exists */
		if (fly_pf_from_parts_with_path(parts, __path, strlen(__path))){
#ifdef DEBUG
			printf("\talready exists: %s\n", __path);
#endif
			continue;
		}
		pfile = fly_pf_init(parts, &sb);
		if (S_ISDIR(sb.st_mode))
			pfile->dir = true;

		memset(pfile->filename, '\0', FLY_PATH_MAX);
		__fly_path_cpy_with_mp(pfile->filename, __path, mount_point, FLY_PATH_MAX);
		pfile->filename_len = strlen(pfile->filename);
		pfile->filename[pfile->filename_len] = '\0';
#ifdef DEBUG
		printf("%s: %s\n", pfile->dir ? "DIR": "FILE", pfile->filename);
#endif
		pfile->mime_type = fly_mime_type_from_path_name(__path);
#ifdef HAVE_INOTIFY
		if (infd >= 0){
			if (pfile->dir)
				pfile->wd = inotify_add_watch(infd, __path, FLY_INOTIFY_WATCH_FLAG_MP);
			else
				pfile->wd = inotify_add_watch(infd, __path, FLY_INOTIFY_WATCH_FLAG_PF);
			if (pfile->wd == -1)
				goto error;
#ifdef DEBUG
			printf("\tADD WATCH ! %s\n", pfile->filename);
#endif
		}else{
			pfile->wd = -1;
		}
#elif defined HAVE_KQUEUE
		if (event != NULL && \
				fly_inotify_kevent_event(event, pfile) == -1)
			goto error;
#endif

		if (fly_hash_from_parts_file_path(__path, pfile) == -1)
			goto error;

		/* add rbnode to rbtree */
		fly_rbdata_t data, key, cmpdata;

		fly_rbdata_set_ptr(&data, pfile);
		fly_rbdata_set_ptr(&key, pfile->filename);
		fly_rbdata_set_size(&cmpdata, pfile->filename_len);
		pfile->rbnode = fly_rb_tree_insert(parts->mount->rbtree, &data, &key, &pfile->rbnode, &cmpdata);
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
	//char rpath[FLY_PATH_MAX];
	char *rpath;

	if (!ctx || !ctx->mount){
#ifdef DEBUG
		printf("MOUNT ERROR: context/mount is invalid.\n");
#endif
		return -1;
	}
	if (path == NULL || strlen(path) > FLY_PATH_MAX){
#ifdef DEBUG
		printf("MOUNT ERROR: path length.\n");
#endif
		return FLY_EARG;
	}

	if (fly_mount_max_limit() == ctx->mount->mount_count)
		return FLY_EMOUNT_LIMIT;

	/* alloc realpath memory */
	rpath = realpath(path, NULL);
	if (rpath == NULL)
		return -1;
	mnt = ctx->mount;

	if (fly_isdir(rpath) != 1)
		return FLY_EARG;

	pool = ctx->pool;
	parts = fly_pballoc(pool, sizeof(fly_mount_parts_t));
	if (fly_unlikely_null(parts))
		return -1;

	if (strlen(rpath) > FLY_PATH_MAX){
		free(rpath);
		return -1;
	}
	__fly_mount_path_cpy(parts->mount_path, rpath);
	/* release realpath memory. */
	free(rpath);
	parts->mount_number = mnt->mount_count;
	parts->mount = mnt;
#ifdef HAVE_INOTIFY
	parts->wd = -1;
	parts->infd = -1;
#elif defined HAVE_KQUEUE
	parts->fd = -1;
#endif
	parts->pool = pool;
	parts->file_count = 0;
	parts->deleted = false;
	fly_bllist_init(&parts->files);

	if (__fly_mount_add(mnt, parts) == -1)
		goto error;
#ifdef HAVE_INOTIFY
	if (__fly_nftw(parts, parts->mount_path, parts->mount_path, -1) == -1){
#elif HAVE_KQUEUE
	if (__fly_nftw(NULL, parts, parts->mount_path, parts->mount_path, -1) == -1){
#else
#error not found inotify or kqueue on your system.
#endif
		goto error;
	}

#ifdef DEBUG
	printf("MOUNT FILE COUNT: %ld\n", ctx->mount->file_count);
	printf("MOUNT RBNODE COUNT: %ld\n", ctx->mount->rbtree->node_count);
	__fly_mount_debug(mnt);
#endif
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
		fly_mount_parts_t *__p;
		struct fly_bllist *__b;

		fly_for_each_bllist(__b, &mnt->parts){
			__p = fly_bllist_data(__b, fly_mount_parts_t, mbelem);
			/* if same mount point, ignore */
			if (strcmp(__p->mount_path, path) == 0){
				fly_bllist_remove(__b);
				fly_pbfree(__p->pool, __p);
				mnt->mount_count--;
				goto check_total_mount_count;
			}
		}
	}
	/* not found */
	return 0;
check_total_mount_count:
#ifdef DEBUG
	assert(mnt != NULL);
	assert(mnt->ctx != NULL);
	assert(mnt->ctx->log != NULL);
#endif
	/* no mount point */
	if (mnt->mount_count == 0)
		/* emergency error. log and end process. */
		FLY_EMERGENCY_ERROR(
			"There is no mount point."
		);
	else
		FLY_NOTICE_DIRECT_LOG(
			mnt->ctx->log,
			"Unmount %s. goodbye.\n", path
		);
	return 0;
}

int fly_mount_number(fly_mount_t *mnt, const char *path)
{
	if (!mnt || !mnt->mount_count)
		return FLY_EARG;

	fly_mount_parts_t *__p;
	struct fly_bllist *__b;
	fly_for_each_bllist(__b, &mnt->parts){
		__p = fly_bllist_data(__b, fly_mount_parts_t, mbelem);
		if (strcmp(__p->mount_path, path) == 0)
			return __p->mount_number;
	}
	return FLY_ENOTFOUND;
}

int fly_join_path(char *buffer, size_t buflen, char *join1, char *join2)
{
	char *ptr, *rpath;
	char result[buflen];
	if (strlen(join1)+strlen(join2)+1 >= buflen)
		return -1;
	ptr = result;
	strcpy(ptr, join1);
	ptr += strlen(join1);
	strcpy(ptr, "/");
	ptr += 1;
	strcpy(ptr, join2);
	ptr += strlen(join2);
	strcpy(ptr, "\0");

	rpath = realpath(result, NULL);
	if (rpath == NULL)
		return -1;

	memset(buffer, '\0', buflen);
	memcpy(buffer, rpath, fly_min((size_t) buflen-1, strlen(rpath)));

	free(rpath);
	return 0;
}

#define FLY_FOUND_CONTENT_FROM_PATH_FOUND		1
#define FLY_FOUND_CONTENT_FROM_PATH_NOTFOUND	0
#define FLY_FOUND_CONTENT_FROM_PATH_ERROR		-1
__fly_unused __fly_static int __fly_uri_matching(char *filename, fly_uri_t *uri)
{
	size_t i=0, j=0, uri_len;
	char *uri_str;

	/* ignore first some slash */
	while (uri->ptr[i] == '/')
		i++;

	if (uri->ptr[i] == '\0' || i>=uri->len){
		uri_str = (char *) fly_index_path();
		uri_len = strlen(uri_str);
	}else{
		uri_str = uri->ptr+i;
		uri_len = uri->len-i;
	}

	while(j<strlen(filename) && j<uri_len && filename[i] == uri_str[i]){
		if (filename[i] == '\0' && uri_str[i] == '\0')
			return 0;
		i++;
		j++;
	}

	return -1;
}

int fly_found_content_from_path(fly_mount_t *mnt, fly_uri_t *uri, struct fly_mount_parts_file **res)
{
	struct fly_mount_parts_file *__pf;
	char *filename;
	size_t len;
	fly_rbdata_t cmpdata, k1, *__d;

	if (fly_unlikely_null(mnt) || mnt->file_count == 0){
		*res = NULL;
		return FLY_FOUND_CONTENT_FROM_PATH_NOTFOUND;
	}

	filename = uri->ptr;
	len = uri->len;
	while(fly_slash(*filename)){
		filename++;
		len--;
	}

	if (len <= 0){
		filename = (char *) fly_index_path();
		len = strlen(filename);
	}

	fly_rbdata_set_size(&cmpdata, len);
	fly_rbdata_set_ptr(&k1, filename);
	__d = fly_rb_node_data_from_key(mnt->rbtree, &k1, &cmpdata);
	if (__d == NULL){
		*res = NULL;
		return FLY_FOUND_CONTENT_FROM_PATH_NOTFOUND;
	}

	__pf = (struct fly_mount_parts_file *) fly_rbdata_ptr(__d);
	if (__pf->dir){
		/* Search index.html in directory */
		int res;
		char dirindex[FLY_PATH_MAX];

		memset(dirindex, '\0', FLY_PATH_MAX);
		res = snprintf(dirindex, FLY_PATH_MAX, "%s/%s", filename, fly_index_path());
		if (res < 0 || res == FLY_PATH_MAX)
			goto not_found;

		fly_rbdata_set_size(&cmpdata, res);
		fly_rbdata_set_ptr(&k1, dirindex);
		__d = fly_rb_node_data_from_key(mnt->rbtree, &k1, &cmpdata);
		if (__d == NULL)
			goto not_found;
	}
#ifdef DEBUG
	assert(__pf != NULL);
#endif
	*res = __pf;
	return FLY_FOUND_CONTENT_FROM_PATH_FOUND;
	FLY_NOT_COME_HERE

not_found:
	*res = NULL;
	return FLY_FOUND_CONTENT_FROM_PATH_NOTFOUND;
}

#ifdef HAVE_INOTIFY
int fly_mount_inotify(fly_mount_t *mount, int ifd)
{
	struct fly_bllist *__b, *__pfb;
	fly_mount_parts_t *parts;
	struct fly_mount_parts_file *__pf;

	fly_for_each_bllist(__b, &mount->parts){
		parts = fly_bllist_data(__b, fly_mount_parts_t, mbelem);
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
		fly_for_each_bllist(__pfb, &parts->files){
			__pf = fly_bllist_data(__pfb, struct fly_mount_parts_file, blelem);
			int wd;
			char rpath[FLY_PATH_MAX];

			if (fly_join_path(rpath, FLY_PATH_MAX, parts->mount_path, __pf->filename) == -1)
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

#elif defined HAVE_KQUEUE
#include "event.h"

int __fly_mount_inotify_kevent_dir(
	struct fly_mount_parts *parts,
	fly_event_manager_t *__m, int fd, void *data,
	fly_event_handler_t *inotify_handler,
	fly_event_handler_t *end_handler
)
{
	struct fly_event *__e;

	__e = fly_event_init(__m);
	__e->fd = fd;
	__e->read_or_write = FLY_KQ_INOTIFY;
	fly_time_null(__e->timeout);
	__e->eflag =FLY_INOTIFY_WATCH_FLAG_MP;
	__e->flag = FLY_PERSISTENT;
	__e->tflag = FLY_INFINITY;
	//__e->event_data = data;
	fly_event_data_set(__e, __p, data);
	FLY_EVENT_HANDLER(__e, inotify_handler);
	fly_event_inotify(__e);
	__e->end_handler = end_handler;

	parts->event = __e;
	parts->fd = fd;
	return fly_event_register(__e);
}

int __fly_mount_inotify_kevent_file(
	struct fly_mount_parts_file *__pf,
	fly_event_manager_t *__m, int fd, void *data,
	fly_event_handler_t *inotify_handler,
	fly_event_handler_t *end_handler
)
{
	struct fly_event *__e;

	__e = fly_event_init(__m);
	__e->fd = fd;
	fly_time_null(__e->timeout);
	__e->read_or_write = FLY_KQ_INOTIFY;
	__e->eflag = FLY_INOTIFY_WATCH_FLAG_PF;
	__e->flag = FLY_PERSISTENT;
	__e->tflag = FLY_INFINITY;
	fly_event_data_set(__e, __p, data);
	FLY_EVENT_HANDLER(__e, inotify_handler);
	fly_event_inotify(__e);
	__e->end_handler = end_handler;

	__pf->event = __e;
	__pf->fd = fd;
	return fly_event_register(__e);
}

int fly_mount_inotify_kevent(
	fly_event_manager_t *manager, fly_mount_t *mount, void *data,
	fly_event_handler_t *handler, fly_event_handler_t *end_handler
)
{
	struct fly_bllist *__b, *__pfb;
	fly_mount_parts_t *parts;
	struct fly_mount_parts_file *__pf;

	fly_for_each_bllist(__b, &mount->parts){
		parts = fly_bllist_data(__b, fly_mount_parts_t, mbelem);

		int fd;
		fd = open(parts->mount_path, O_DIRECTORY|O_RDONLY);
		if (fd == -1)
			return -1;

		parts->fd = fd;
		if (__fly_mount_inotify_kevent_dir( \
				parts, manager, fd, data, handler, end_handler) == -1)
			return -1;

		if (parts->file_count == 0)
			continue;
		/* inotify add watch file */
		fly_for_each_bllist(__pfb, &parts->files){
			__pf = fly_bllist_data(__pfb, struct fly_mount_parts_file, blelem);
			int pfd;
			char rpath[FLY_PATH_MAX];

			if (fly_join_path(rpath, FLY_PATH_MAX, parts->mount_path, __pf->filename) == -1)
				continue;
			pfd = open(rpath, O_RDONLY);
			if (pfd == -1)
				return -1;
			__pf->fd = pfd;

			if (__fly_mount_inotify_kevent_file( \
					__pf, manager, pfd, data, handler, end_handler) == -1)
				return -1;
		}
	}
#ifdef DEBUG
	printf("MOUNT FILE COUNT: %ld\n", mount->file_count);
	printf("MOUNT RBNODE COUNT: %ld\n", mount->rbtree->node_count);
	__fly_mount_debug(mount);
#endif
	return 0;
}
#endif

struct fly_mount_parts_file *fly_pf_from_parts(char *path, size_t path_len, fly_mount_parts_t *parts)
{
	struct fly_mount_parts_file *__pf;
	fly_rbdata_t k1, cmpdata, *node_data;

	if (parts->file_count == 0)
		return NULL;

	fly_rbdata_set_ptr(&k1, path);
	fly_rbdata_set_size(&cmpdata, path_len);
	node_data = fly_rb_node_data_from_key(parts->mount->rbtree, &k1, &cmpdata);
	if (node_data == NULL)
		return NULL;
	else{
		__pf = (struct fly_mount_parts_file *)  fly_rbdata_ptr(node_data);
		return __pf;
	}
	FLY_NOT_COME_HERE
}

struct fly_mount_parts_file *fly_pf_from_parts_by_fullpath(char *path, fly_mount_parts_t *parts)
{
	char __path[FLY_PATH_MAX];
	char *mnt_path;
	size_t path_len;
	__fly_unused fly_mount_t *mnt;

	if (parts->file_count == 0)
		return NULL;

	mnt_path = parts->mount_path;
	mnt = parts->mount;

	while(*path++ == *mnt_path++)
		if (*mnt_path == '\0')
			break;

	while(fly_slash(*path))
		path++;

	path_len = strlen(path);
	memset(__path, '\0', FLY_PATH_MAX);
	memcpy(__path, path, path_len);
	return fly_pf_from_parts(__path, strlen(path), parts);
}

#ifdef HAVE_INOTIFY
struct fly_mount_parts_file *fly_pf_from_wd(int wd, fly_mount_parts_t *parts)
{
	struct fly_mount_parts_file *__pf;
	struct fly_bllist *__b;

	if (parts->file_count == 0)
		return NULL;

	fly_for_each_bllist(__b, &parts->files){
		__pf = fly_bllist_data(__b, struct fly_mount_parts_file, blelem);
		if (__pf->wd == wd)
			return __pf;
	}
	return NULL;
}
#elif defined HAVE_KQUEUE
struct fly_mount_parts_file *fly_pf_from_fd(int fd, fly_mount_parts_t *parts)
{
	struct fly_mount_parts_file *__pf;
	struct fly_bllist *__b;

	if (parts->file_count == 0)
		return NULL;

	fly_for_each_bllist(__b, &parts->files){
		__pf = fly_bllist_data(__b, struct fly_mount_parts_file, blelem);
		if (__pf->fd == fd)
			return __pf;
	}
	return NULL;
}
#endif

#ifdef HAVE_INOTIFY
struct fly_mount_parts_file *fly_pf_from_mount(int wd, fly_mount_t *mnt)
{
	fly_mount_parts_t *__p;
	struct fly_mount_parts_file *pf;
	struct fly_bllist *__b;

	if (mnt->mount_count == 0)
		return NULL;

	fly_for_each_bllist(__b, &mnt->parts){
		__p = fly_bllist_data(__b, fly_mount_parts_t, mbelem);
		pf = fly_pf_from_wd(wd, __p);
		if (pf)
			return pf;
	}
	return NULL;
}
#elif HAVE_KQUEUE
struct fly_mount_parts_file *fly_pf_from_mount(int fd, fly_mount_t *mnt)
{
	fly_mount_parts_t *__p;
	struct fly_mount_parts_file *pf;
	struct fly_bllist *__b;

	if (mnt->mount_count == 0)
		return NULL;

	fly_for_each_bllist(__b, &mnt->parts){
		__p = fly_bllist_data(__b, fly_mount_parts_t, mbelem);
		pf = fly_pf_from_fd(fd, __p);
		if (pf)
			return pf;
	}
	return NULL;
}
#endif

#ifdef HAVE_INOTIFY
fly_mount_parts_t *fly_parts_from_wd(int wd, fly_mount_t *mnt)
{
	fly_mount_parts_t *__p;
	struct fly_bllist *__b;

	if (mnt->mount_count == 0)
		return NULL;

	fly_for_each_bllist(__b, &mnt->parts){
		__p = fly_bllist_data(__b, fly_mount_parts_t, mbelem);
		if (__p->wd == wd)
			return __p;
	}
	return NULL;
}
#endif

#ifdef HAVE_KQUEUE
fly_mount_parts_t *fly_parts_from_fd(int fd, fly_mount_t *mnt)
{
	fly_mount_parts_t *__p;
	struct fly_bllist *__b;

	if (mnt->mount_count == 0)
		return NULL;

	fly_for_each_bllist(__b, &mnt->parts){
		__p = fly_bllist_data(__b, fly_mount_parts_t, mbelem);
		if (__p->fd == fd)
			return __p;
	}
	return NULL;
}
#endif

#ifdef HAVE_INOTIFY
int fly_inotify_add_watch(fly_mount_parts_t *parts, char *path, size_t len)
{
	struct fly_mount_parts_file *__npf;
	char rpath[FLY_PATH_MAX];

	if (fly_join_path(rpath, FLY_PATH_MAX, parts->mount_path, path) == -1)
		return -1;

	if (fly_isdir(rpath)){
#ifdef DEBUG
		printf("\tADD WATCH DIR %s. Go looking in the directory\n", rpath);
#endif
		if (__fly_nftw(parts, (const char *) rpath, parts->mount_path, parts->infd) == -1)
			return -1;
	}

	struct stat sb;
	if (stat(rpath, &sb) == -1)
		return -1;

	__npf = fly_pf_init(parts, &sb);
	__npf->infd = parts->infd;
	if (__npf->dir)
		__npf->wd = inotify_add_watch(__npf->infd, rpath, FLY_INOTIFY_WATCH_FLAG_MP);
	else
		__npf->wd = inotify_add_watch(__npf->infd, rpath, FLY_INOTIFY_WATCH_FLAG_PF);
	if (__npf->wd == -1)
		return -1;
	__npf->parts = parts;
	strncpy(__npf->filename, path, len);
	__npf->filename_len = strlen(__npf->filename);
	if (fly_hash_from_parts_file_path(rpath, __npf) == -1)
		return -1;

	/* add rbtree of mount */
	fly_rbdata_t data, key, cmpdata;

	fly_rbdata_set_ptr(&data, __npf);
	fly_rbdata_set_ptr(&key, __npf->filename);
	fly_rbdata_set_size(&cmpdata, len);
	__npf->rbnode = fly_rb_tree_insert(parts->mount->rbtree, &data, &key, &__npf->rbnode, &cmpdata);
	fly_parts_file_add(parts, __npf);
#ifdef DEBUG
	printf("\tADD WATCH! wd: %d, filename: %s\n", __npf->wd, __npf->filename);
	assert(fly_pf_from_wd(__npf->wd, __npf->parts));
#endif
	return 0;
}

#elif HAVE_KQUEUE
int fly_inotify_kevent_event(fly_event_t *e, struct fly_mount_parts_file *pf)
{
	int fd, res;
	fly_event_t *__e;
	char rpath[FLY_PATH_MAX];
	struct fly_mount_parts *parts;

	parts = pf->parts;
#ifdef DEBUG
	assert(parts != NULL);
#endif

	res = snprintf(rpath, FLY_PATH_MAX, "%s/%s", parts->mount_path, pf->filename);
	if (res < 0 || res == FLY_PATH_MAX)
		return -1;
	fd = open(rpath, O_RDONLY);
	if (fd == -1)
		return -1;
#ifdef DEBUG
	printf("@@@ INOTIFY KEVENT OPEN FILE: %d @@@\n", fd);
#endif

	__e = fly_event_init(e->manager);
	__e->fd = fd;
	__e->read_or_write = FLY_KQ_INOTIFY;
	__e->flag = FLY_PERSISTENT;
	__e->tflag = FLY_INFINITY;
	__e->expired = false;
	__e->available = false;
	fly_time_null(__e->timeout);
	if (pf->dir)
		__e->eflag = FLY_INOTIFY_WATCH_FLAG_MP;
	else
		__e->eflag = FLY_INOTIFY_WATCH_FLAG_PF;
	memcpy(&__e->event_data, &e->event_data, sizeof(fly_event_union));
	memcpy(&__e->end_event_data, &e->end_event_data, sizeof(fly_event_union));
	__e->handler = e->handler;
	__e->handler_name = e->handler_name;
	__e->end_handler = e->end_handler;
	fly_event_inotify(__e);
	pf->event = __e;
	pf->fd = fd;
#ifdef DEBUG
	printf("Added kevent %s\n", rpath);
#endif
	return fly_event_register(__e);
}

static int __fly_same_direntry(fly_mount_parts_t *parts, struct dirent *__de)
{
	struct fly_mount_parts_file *__npf;
	struct fly_bllist *__b;

	fly_for_each_bllist(__b, &parts->files){
		__npf = (struct fly_mount_parts_file *) fly_bllist_data(__b, struct fly_mount_parts_file, blelem);
		/*
		 *	continue if Directory and File.
		 */
		if (__de->d_type == DT_DIR && !__npf->dir)
			continue;
		if (__de->d_type == DT_REG && __npf->dir)
			continue;

		/* if same entry, continue */
		if (strncmp(__de->d_name, __npf->filename, strlen(__de->d_name)) == 0){
			return 1;
		}
	}
	return 0;
}

int fly_inotify_add_watch(fly_mount_parts_t *parts, fly_event_t *__e)
{
	int res;
	DIR *__d;
	struct dirent *__de;
	char rpath[FLY_PATH_MAX];
	struct fly_mount_parts_file *__npf;

	__d = opendir(parts->mount_path);
	if (__d == NULL)
		return -1;
	while((__de=readdir(__d)) != NULL){
#ifdef DEBUG
		printf("\t%s: %s\n", parts->mount_path, __de->d_name);
#endif
		/* ignore "." or ".." directories. */
		if (strncmp(__de->d_name, ".", strlen(".")) == 0 || \
				strncmp(__de->d_name, "..", strlen("..")) == 0)
			continue;

		if (strlen(__de->d_name) >= FLY_PATHNAME_MAX){
#ifdef DEBUG
			printf("\tpath name too long. %ld(max: %dcharacters)\n", strlen(__de->d_name), FLY_PATHNAME_MAX);
#endif
			continue;
		}
		res = snprintf(rpath, FLY_PATH_MAX, "%s/%s", parts->mount_path, __de->d_name);
		if (res < 0 || res == FLY_PATH_MAX){
#ifdef DEBUG
			printf("\tinvalid path name.\n");
#endif
			continue;
		}

		if (fly_isdir(rpath)){
#ifdef DEBUG
			printf("\t%s is directory. will nftw\n", rpath);
#endif
			if (__fly_nftw(__e, parts, (const char *) rpath, parts->mount_path, parts->fd) == -1)

				goto error;
		}
		/* already exists parts file/directory. */
		if (__fly_same_direntry(parts, __de)){
#ifdef DEBUG
			printf("\tsame_direntry %s: %s\n", parts->mount_path, __de->d_name);
#endif
			continue;
		}

		struct stat sb;
		if (stat(rpath, &sb) == -1)
			goto error;

		__npf = fly_pf_init(parts, &sb);
		__npf->parts = parts;
		__fly_path_cpy_with_mp(__npf->filename, rpath, (const char *) parts->mount_path, FLY_PATH_MAX);
		__npf->filename_len = strlen(__npf->filename);
		if (fly_hash_from_parts_file_path(rpath, __npf) == -1)
			goto error_pf;

		/* event of kevent make */
		if (fly_inotify_kevent_event(__e, __npf) == -1)
			goto error_pf;

		/* add rbtree of mount */
		fly_rbdata_t data, key, cmpdata;

		fly_rbdata_set_ptr(&data, __npf);
		fly_rbdata_set_ptr(&key, __npf->filename);
		fly_rbdata_set_size(&cmpdata, strlen(__npf->filename));
		__npf->rbnode = fly_rb_tree_insert(parts->mount->rbtree, &data, &key, &__npf->rbnode, &cmpdata);
		fly_parts_file_add(parts, __npf);
#ifdef DEBUG
		printf("\tADD WATCH! fd: %d, event fd: %d\n", __npf->fd, __npf->event->fd);
		assert(fly_pf_from_fd(__npf->event->fd, parts) != NULL);
#endif
		FLY_NOTICE_DIRECT_LOG(parts->mount->ctx->log,
				"Master detected a new %s(%s) at %s. start watching.\n",
				__npf->dir ? "directory" : "file",
				__npf->filename, parts->mount_path
		);
		continue;
error_pf:
		fly_pf_release(__npf);
		goto error;
	}

	if (closedir(__d) == -1)
		return -1;
	return 0;
error:
	if (closedir(__d) == -1)
		return -1;
	return -1;
}
#endif

__fly_unused static int __fly_samedir_cmp(char *s1, char *s2)
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
	struct fly_bllist *__b, *__n;
	struct fly_mount_parts_file *__pf;
#ifdef DEBUG
	int __tmp;
	size_t __tmpm;

	__tmp = parts->file_count;
	__tmpm = parts->mount->file_count;
#endif

	/* remove parts file of mount point */
	for (__b=parts->files.next; __b!=&parts->files; __b=__n){
#ifdef HAVE_KQUEUE
		fly_event_t *__e;
#endif
		__n = __b->next;
		__pf = fly_bllist_data(__b, struct fly_mount_parts_file, blelem);
#if defined HAVE_KQUEUE && defined DEBUG
		assert(__pf->fd > 0);
		assert(__pf->event != NULL);
#endif
#ifdef HAVE_KQUEUE
		__e = __pf->event;
#endif
		if (fly_inotify_rm_watch(__pf) == -1)
			return -1;
#ifdef HAVE_KQUEUE
		if (fly_event_unregister(__e) == -1)
			return -1;
#endif
	}
#ifdef DEBUG
	/*
	 * previous mount's file count == \
	 * now it's file count + unmount mount parts file count
	 */
	assert(__tmpm == parts->mount->file_count + __tmp);
#endif

	/* remove mount point */
#ifdef HAVE_INOTIFY
	/*
	 * If mount point is alive, do inotify_rm_watch.
	 */
	if (!parts->deleted && \
			inotify_rm_watch(parts->infd, parts->wd) == -1)
		return -1;
#elif defined HAVE_KQUEUE
	if (close(parts->fd) == -1)
		return -1;
#ifdef DEBUG
	printf("INOTIFY RMMP %s:fd %d\n", parts->mount_path, parts->fd);
#endif
	parts->event->flag = FLY_CLOSE_EV;
#endif
	if (fly_unmount(parts->mount, parts->mount_path) == -1)
		return -1;

	return 0;
}

int fly_inotify_rm_watch(struct fly_mount_parts_file *pf)
{
#ifdef DEBUG
	assert(pf != NULL);
#ifdef HAVE_KQUEUE
	assert(pf->event != NULL);
	assert(pf->event->read_or_write == FLY_KQ_INOTIFY);
#endif
	assert(pf->parts->file_count > 0);
#endif
	if (pf->parts->file_count == 0)
		return 0;

#ifdef DEBUG
	printf("MASTER: %s %s: %d\n", pf->filename, __FILE__, __LINE__);
#endif
	if (pf->dir){
		struct fly_bllist *__b, *__n;
		struct fly_mount_parts_file *__pf;
		char *dirptr;
		size_t dirlen;

		dirptr = pf->filename;
		if (strchr(dirptr, '/') == NULL){
			dirlen = pf->filename_len;
		}else{
			while(strchr(dirptr, '/') != NULL)
				dirptr = strchr(dirptr, '/');

			dirlen = dirptr-pf->filename+1;
		}

		for (__b=pf->parts->files.next; __b!=&pf->parts->files; __b=__n){
			__n = __b->next;
			__pf = (struct fly_mount_parts_file *) fly_bllist_data(__b, struct fly_mount_parts_file, blelem);
			/* same directory but not directory */
			if (dirlen != __pf->filename_len && \
					strncmp(__pf->filename, pf->filename, dirlen) == 0){
#ifdef HAVE_KQUEUE
				fly_event_t *__etmp;
				__etmp = __pf->event;
#endif
				if (fly_inotify_rm_watch(__pf) == -1)
					return -1;
#ifdef HAVE_KQUEUE
#ifdef DEBUG
				assert(__pf->event->flag == FLY_CLOSE_EV);
#endif
				if (fly_event_unregister(__etmp) == -1)
					return -1;
#endif
			}
		}
	}
#ifdef DEBUG
	printf("\t\"%s\"(fd %d): release resources\n", pf->filename, pf->fd);
	printf("%s is \"%s\"\n", pf->filename, pf->deleted ? "DELETED": "ALIVE");
#endif
#ifdef HAVE_INOTIFY
	/*
	 * If watching file is alive, do inotify_rm_watch.
	 */
#ifdef DEBUG
#endif
	if (!pf->deleted && \
			inotify_rm_watch(pf->parts->infd, pf->wd) == -1)
		return -1;
#elif defined HAVE_KQUEUE
	pf->event->flag = FLY_CLOSE_EV;
	fly_event_manager_remove_event(pf->event);
	if (close(pf->fd) == -1)
		return -1;
#ifdef DEBUG
	printf("INOTIFY RMWATCH %s:fd %d\n", pf->filename, pf->fd);
	assert(close(pf->fd) == -1 && errno == EBADF);
#endif
#endif
	fly_parts_file_remove(pf->parts, pf);
	return 0;
}

int fly_mount_files_count(fly_mount_t *mnt, int mount_number)
{
	struct fly_bllist *__b;
	struct fly_mount_parts *__p;

	fly_for_each_bllist(__b, &mnt->parts){
		__p = fly_bllist_data(__b, struct fly_mount_parts, mbelem);
		if (__p->mount_number == mount_number)
			return __p->file_count;
	}
	/* not found */
	return -1;
}

#include "conf.h"
static int fly_mount_max_limit(void)
{
	return fly_config_value_int(FLY_MOUNT_MAX);
}

static size_t fly_file_max_limit(void)
{
	return (size_t) fly_config_value_int(FLY_FILE_MAX);
}

/*
 *	data is length of k1 path.
 */
static int __fly_mount_search_cmp(fly_rbdata_t *k1, fly_rbdata_t *k2, fly_rbdata_t *cmpdata)
{
	char *c1, *c2;
	int res;
	size_t minlen;
	size_t len;

	c1 = (char *) fly_rbdata_ptr(k1);
	c2 = (char *) fly_rbdata_ptr(k2);
	/* c1 length */
	len = fly_rbdata_size(cmpdata);

	minlen = (strlen(c2) < len) ? strlen(c2) : len;
	res = strncmp(c1, c2, minlen);

	if (res == 0){
		if (strlen(c2) == len)
			return FLY_RB_CMP_EQUAL;
		else{
			if (strlen(c2) < len)
				return FLY_RB_CMP_BIG;
			else
				return FLY_RB_CMP_SMALL;
		}
	}else if (res > 0)
		return FLY_RB_CMP_BIG;
	else
		return FLY_RB_CMP_SMALL;

	FLY_NOT_COME_HERE
}


const char *fly_index_path(void)
{
	return (const char *) fly_config_value_str(FLY_INDEX_PATH);
}

#ifdef DEBUG
void __fly_debug_mnt_content(fly_context_t *ctx)
{
	assert(ctx != NULL);
	assert(ctx->mount != NULL);
	struct fly_mount_parts *__p;
	struct fly_bllist *__b;

	printf("==============================\n");
	fly_for_each_bllist(__b, &ctx->mount->parts){
		struct fly_mount_parts_file *__pf;
		struct fly_bllist * __pb;

		__p = (struct fly_mount_parts *) fly_bllist_data(__b, fly_mount_parts_t, mbelem);
		printf("MOUNT POINT: %s\n", __p->mount_path);

		fly_for_each_bllist(__pb, &__p->files){
			__pf = (struct fly_mount_parts_file *) fly_bllist_data(__pb, struct fly_mount_parts_file, blelem);
			printf("\tWATCHING: %s\n", __pf->filename);
		}
	}
	printf("==============================\n");
}
#endif

#ifdef HAVE_KQUEUE
__fly_unused int fly_parts_set_newdir(fly_mount_parts_t *parts, DIR *dir)
{
	int fd;

	fd = dirfd(dir);
	/* close old fd, and set new fd*/
	if (close(parts->fd) == -1)
		return -1;
	parts->fd = fd;
	return 0;
}
#endif
