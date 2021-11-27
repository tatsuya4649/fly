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
static int fly_file_max_limit(void);
static int __fly_mount_search_cmp(void *k1, void *k2, void *data);

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

void fly_parts_file_remove(fly_mount_parts_t *parts, struct fly_mount_parts_file *pf)
{
	fly_bllist_remove(&pf->blelem);
	if (pf->rbnode != NULL)
		fly_rb_delete(parts->mount->rbtree, pf->rbnode);
	fly_pbfree(parts->mount->ctx->pool, pf);
	parts->file_count--;
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
#ifdef HAVE_INOTIFY
	pfile->fd = -1;
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
	pfile->dir = false;
	pfile->rbnode = NULL;
	pfile->overflow = false;

	return pfile;
}

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
		if (res < 0 || res == FLY_PATH_MAX)
			continue;
		if (strlen(__ent->d_name) >= FLY_PATHNAME_MAX)
			continue;

		if (stat(__path, &sb) == -1)
			continue;
		/* recursion */
		if (S_ISDIR(sb.st_mode))
#ifdef HAVE_INOTIFY
			if (__fly_nftw(parts, __path, mount_point, infd) == -1)
#elif defined HAVE_KQUEUE
			if (__fly_nftw(event, parts, __path, mount_point, infd) == -1)
#else
#error  not found inotify or kqueue on your system
#endif
				goto error;

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
			if (strcmp(path, mount_point) == 0)
				pfile->wd = inotify_add_watch(infd, __path, FLY_INOTIFY_WATCH_FLAG_MP);
			else
				pfile->wd = inotify_add_watch(infd, __path, FLY_INOTIFY_WATCH_FLAG_PF);
			if (pfile->wd == -1)
				goto error;
		}else
			pfile->wd = -1;
#elif defined HAVE_KQUEUE
		if (event != NULL && \
				fly_inotify_kevent_event(event, pfile) == -1)
			return -1;
#endif

		if (fly_hash_from_parts_file_path(__path, pfile) == -1)
			goto error;

		/* add rbnode to rbtree */
		pfile->rbnode = fly_rb_tree_insert(parts->mount->rbtree, (void *) pfile, (void *) pfile->filename, &pfile->rbnode, (void *) pfile->filename_len);
		fly_parts_file_add(parts, pfile);
		parts->mount->file_count++;
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
	if (fly_mount_max_limit() == ctx->mount->mount_count)
		return FLY_EMOUNT_LIMIT;

	if (realpath(path, rpath) == NULL)
		return -1;
	mnt = ctx->mount;

	if (path == NULL || strlen(path) > FLY_PATH_MAX)
		return FLY_EARG;
	if (fly_isdir(rpath) != 1)
		return FLY_EARG;

	pool = ctx->pool;
	parts = fly_pballoc(pool, sizeof(fly_mount_parts_t));
	if (fly_unlikely_null(parts))
		return -1;

	__fly_mount_path_cpy(parts->mount_path, rpath);
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
	fly_bllist_init(&parts->files);

	if (__fly_mount_add(mnt, parts) == -1)
		goto error;
#ifdef HAVE_INOTIFY
	if (__fly_nftw(parts, rpath, rpath, -1) == -1)
#elif HAVE_KQUEUE
	if (__fly_nftw(NULL, parts, rpath, rpath, -1) == -1)
#else
#error not found inotify or kqueue on your system.
#endif
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
	/* no mount point */
	if (mnt->mount_count == 0)
		/* emergency error. log and end process. */
		FLY_EMERGENCY_ERROR(
			"There is no mount point."
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

	__pf = (struct fly_mount_parts_file *) \
				fly_rb_node_data_from_key(mnt->rbtree, filename, (void *) len);

	if (__pf != NULL){
		*res = __pf;
		return FLY_FOUND_CONTENT_FROM_PATH_FOUND;
	}else{
		*res = NULL;
		return FLY_FOUND_CONTENT_FROM_PATH_NOTFOUND;
	}
	FLY_NOT_COME_HERE
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

#elif defined HAVE_KQUEUE
#include "event.h"

int __fly_mount_inotify_kevent_dir(
	struct fly_mount_parts *parts,
	fly_event_manager_t *__m, int fd, void *data,
	fly_event_handler_t *handler,
	fly_event_handler_t *end_handler
)
{
	struct fly_event *__e;

	__e = fly_event_init(__m);
	__e->fd = fd;
	__e->read_or_write = FLY_KQ_INOTIFY;
	fly_time_null(__e->timeout);
	__e->eflag = NOTE_DELETE|NOTE_EXTEND|NOTE_LINK;
	__e->flag = FLY_PERSISTENT;
	__e->tflag = FLY_INFINITY;
	__e->event_data = data;
	FLY_EVENT_HANDLER(__e, handler);
	fly_event_inotify(__e);
	__e->end_handler = end_handler;

	parts->event = __e;
	
	return fly_event_register(__e);
}

int __fly_mount_inotify_kevent_file(
	struct fly_mount_parts_file *__pf,
	fly_event_manager_t *__m, int fd, void *data,
	fly_event_handler_t *handler,
	fly_event_handler_t *end_handler
)
{
	struct fly_event *__e;

	__e = fly_event_init(__m);
	__e->fd = fd;
	fly_time_null(__e->timeout);
	__e->eflag = NOTE_DELETE|NOTE_EXTEND|NOTE_ATTRIB|NOTE_CLOSE_WRITE|NOTE_RENAME;
	__e->flag = FLY_PERSISTENT;
	__e->tflag = FLY_INFINITY;
	__e->event_data = data;
	FLY_EVENT_HANDLER(__e, handler);
	fly_event_inotify(__e);
	__e->end_handler = end_handler;

	__pf->event = __e;
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

			if (fly_join_path(rpath, parts->mount_path, __pf->filename) == -1)
				continue;
			pfd = open(rpath, O_RDONLY);
			if (pfd == -1)
				return -1;

			if (__fly_mount_inotify_kevent_file( \
					__pf, manager, pfd, data, handler, end_handler) == -1)
				return -1;
		}
	}
	return 0;
}
#endif

struct fly_mount_parts_file *fly_pf_from_parts(char *path, size_t path_len, fly_mount_parts_t *parts)
{
	struct fly_mount_parts_file *__pf;

	if (parts->file_count == 0)
		return NULL;

	__pf = fly_rb_node_data_from_key(parts->mount->rbtree, path, &path_len);
	return __pf;
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
		pf = fly_wd_from_pf(wd, __p);
		if (pf)
			return pf;
	}
	return NULL;
}
#elif HAVE_KQUEUE
struct fly_mount_parts_file *fly_mount_from_fd(int fd, fly_mount_t *mnt)
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

#ifdef HAVE_INOTIFY
int fly_inotify_add_watch(fly_mount_parts_t *parts, char *path, size_t len)
{
	struct fly_mount_parts_file *__npf;
	char rpath[FLY_PATH_MAX];

	if (fly_join_path(rpath, parts->mount_path, path) == -1)
		return -1;

	if (fly_isdir(rpath)){
		if (__fly_nftw(parts, (const char *) rpath, parts->mount_path, parts->infd) == -1)
			return -1;
	}else{
		struct stat sb;

		if (stat(rpath, &sb) == -1)
			return -1;

		__npf = fly_pf_init(parts, &sb);
		if (fly_unlikely_null(__npf))
			return -1;

		__npf->infd = parts->infd;
		__npf->wd = inotify_add_watch(__npf->infd, rpath, FLY_INOTIFY_WATCH_FLAG_PF);
		__npf->parts = parts;
		strncpy(__npf->filename, path, len);
		__npf->filename_len = strlen(__npf->filename);
		if (fly_hash_from_parts_file_path(rpath, __npf) == -1)
			return -1;

		__npf->rbnode = fly_rb_tree_insert(parts->mount->rbtree, (void *) __npf, (void *) __npf->filename, &__npf->rbnode, (void *) len);
		fly_parts_file_add(parts, __npf);
		parts->mount->file_count++;
	}
	return 0;
}
#elif HAVE_KQUEUE

int fly_inotify_kevent_event(fly_event_t *e, struct fly_mount_parts_file *pf)
{
	int fd;
	fly_event_t *__e;

	fd = open(pf->filename, O_RDONLY);
	if (fd == -1)
		return -1;

	__e = fly_event_init(e->manager);
	__e->fd = fd;
	__e->read_or_write = FLY_KQ_INOTIFY;
	fly_time_null(__e->timeout);
	if (pf->dir)
		__e->eflag = FLY_INOTIFY_WATCH_FLAG_MP;
	else
		__e->eflag = FLY_INOTIFY_WATCH_FLAG_PF;
	FLY_EVENT_HANDLER(__e, e->handler);
	__e->end_handler = e->end_handler;
	fly_event_inotify(__e);
	pf->event = __e;
	return fly_event_register(__e);
}

static int __fly_same_direntry(fly_mount_parts_t *parts, struct dirent *__de)
{
	struct fly_mount_parts_file *__npf;
	struct fly_bllist *__b;

	fly_for_each_bllist(__b, &parts->files){
		__npf = (struct fly_mount_parts_file *) fly_bllist_data(__b, struct fly_mount_parts_file, blelem);
		/* if same entry, continue */
		if (strncmp(__de->d_name, __npf->filename, strlen(__de->d_name)) == 0)
			return 1;
	}
	return 0;
}

int fly_inotify_add_watch(fly_mount_parts_t *parts, fly_event_t *__e)
{
	DIR *__d;
	struct dirent *__de;
	char rpath[FLY_PATH_MAX];
	struct fly_mount_parts_file *__npf;

	__d = opendir(parts->mount_path);
	if (__d == NULL)
		return -1;

	while((__de=readdir(__d)) != NULL){
		if (__fly_same_direntry(parts, __de))
			continue;

		if (fly_join_path(rpath, parts->mount_path, __de->d_name) == -1)
			return -1;

		if (fly_isdir(rpath)){
			if (__fly_nftw(__e, parts, (const char *) rpath, parts->mount_path, parts->fd) == -1)
				return -1;
		}else{
			struct stat sb;

			if (stat(rpath, &sb) == -1)
				return -1;

			__npf = fly_pf_init(parts, &sb);
			if (fly_unlikely_null(__npf))
				return -1;

			__npf->parts = parts;
			__fly_path_cpy_with_mp(__npf->filename, rpath, (const char *) parts->mount_path, FLY_PATH_MAX);
			__npf->filename_len = strlen(__npf->filename);
			if (fly_hash_from_parts_file_path(rpath, __npf) == -1)
				return -1;

			/* event of kevent make */
			if (fly_inotify_kevent_event(__e, __npf) == -1)
				return -1;

			__npf->rbnode = fly_rb_tree_insert(parts->mount->rbtree, (void *) __npf, (void *) __npf->filename, &__npf->rbnode, (void *) strlen(__npf->filename));
			fly_parts_file_add(parts, __npf);
			parts->mount->file_count++;
		}
	}
	return 0;
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

#ifdef HAVE_INOTIFY
int fly_inotify_rmmp(fly_mount_parts_t *parts)
{
	struct fly_bllist *__b;
	struct fly_mount_parts_file *__pf;

	/* remove mount point */
	fly_for_each_bllist(__b, &parts->files){
		__pf = fly_bllist_data(__b, struct fly_mount_parts_file, blelem);
		if (__pf->infd > 0 && __pf->wd > 0)
			if(inotify_rm_watch(__pf->infd, __pf->wd) == -1)
				return -1;

		fly_parts_file_remove(parts, __pf);
		parts->mount->file_count--;
	}

	struct stat statb;
	if (stat(parts->mount_path, &statb) == 0 && \
			inotify_rm_watch(parts->infd, parts->wd) == -1)
		return -1;

	if (fly_unmount(parts->mount, parts->mount_path) == -1)
		return -1;
	return 0;
}
#endif

#ifdef HAVE_INOTIFY
int fly_inotify_rm_watch(fly_mount_parts_t *parts, char *path, size_t path_len, int mask)
{
	/* remove mount point elements */
	if (parts->file_count == 0)
		return -1;

	struct fly_mount_parts_file *__pf;

	__pf = fly_pf_from_parts(path, path_len, parts);
	if (!__pf)
		return 0;

	if (mask & IN_MOVED_FROM && \
			inotify_rm_watch(__pf->infd, __pf->wd) == -1)
		return -1;

	fly_parts_file_remove(parts, __pf);
	parts->mount->file_count--;
	return 0;
}
#elif HAVE_KQUEUE
int fly_inotify_rm_watch(struct fly_mount_parts_file *pf, int mask __fly_unused)
{
#ifdef DEBUG
	assert(pf != NULL);
#endif
	struct fly_mount *__m;
	/* remove mount point elements */
	if (pf->parts->file_count == 0)
		return -1;

	__m = pf->parts->mount;
	
	if (close(pf->fd) == -1)
		return -1;

	pf->event->flag = FLY_CLOSE_EV;
	if (fly_event_unregister(pf->event) == -1)
		return -1;

	fly_parts_file_remove(pf->parts, pf);
	__m->file_count--;
	return 0;
}
#endif

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

static int fly_file_max_limit(void)
{
	return fly_config_value_int(FLY_FILE_MAX);
}

/*
 *	data is length of k1 path.
 */
static int __fly_mount_search_cmp(void *k1, void *k2, void *data)
{
	char *c1, *c2;
	int res;
	size_t minlen;
	size_t len;

	c1 = (char *) k1;
	c2 = (char *) k2;
	len = (size_t) data;

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
