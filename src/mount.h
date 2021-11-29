#ifndef _MOUNT_H
#define _MOUNT_H

#include "../config.h"
#include <dirent.h>
#include <limits.h>
#include <sys/stat.h>
#ifdef HAVE_INOTIFY
#include <sys/inotify.h>
#endif
#include "alloc.h"
#include "header.h"
#include "mime.h"
#include "bllist.h"
#include "encode.h"
#include "rbtree.h"
#include "str.h"

#define FLY_PATHNAME_MAX	_POSIX_NAME_MAX
#define FLY_PATH_MAX	_POSIX_PATH_MAX
#define FLY_MOUNT_POOL_PAGE		((fly_page_t) 10)
#define FLY_MOUNT_INIT_NUMBER		0
#define FLY_DATE_LENGTH			(50)
#define FLY_MOUNT_MAX				"FLY_MOUNT_MAX"
#define FLY_FILE_MAX				"FLY_FILE_MAX"

#define FLY_MOUNT_DEFAULT_ENCODE_TYPE			fly_gzip
struct fly_mount_parts_file{
	int						fd;
#ifdef HAVE_INOTIFY
	int 					wd;
	int 					infd;
#elif HAVE_KQUEUE
	struct fly_event		*event;
#endif
	struct stat				fs;
	char					filename[FLY_PATH_MAX];
	size_t					filename_len;
	char 					last_modified[FLY_DATE_LENGTH];
	struct fly_mount_parts	*parts;
	struct fly_file_hash	*hash;
	fly_mime_type_t			*mime_type;
	struct fly_rb_node		*rbnode;

	struct fly_bllist		blelem;

	fly_encoding_e			encode_type;
	struct fly_de			*de;
	fly_bit_t				encoded: 1;
	fly_bit_t				dir: 1;
	fly_bit_t				overflow: 1;
};

struct fly_mount_parts{
#ifdef HAVE_INOTIFY
	int						wd;
	int 					infd;
#elif defined HAVE_KQUEUE
	int						fd;
#endif
	char					mount_path[FLY_PATH_MAX];
	int						mount_number;
#ifdef HAVE_KQUEUE
	struct fly_event 		*event;
#endif

	struct fly_bllist		files;
	int						file_count;

	struct fly_bllist		mbelem;
	struct fly_mount		*mount;
	fly_pool_t				*pool;
};

struct fly_context;
typedef struct fly_context fly_context_t;
struct fly_mount{
	struct fly_bllist			parts;
	int							mount_count;
	int 						file_count;

	struct fly_mount_parts_file	*index;
	struct fly_rb_tree			*rbtree;

	fly_context_t *ctx;
};
typedef struct fly_mount fly_mount_t;
typedef struct fly_mount_parts fly_mount_parts_t;

int fly_mount_init(fly_context_t *ctx);
void fly_mount_release(fly_context_t *ctx);
int fly_mount(fly_context_t *ctx, const char *path);
int fly_unmount(fly_mount_t *mnt, const char *path);

int fly_isdir(const char *path);
int fly_isfile(const char *path);
ssize_t fly_file_size(const char *path);
int fly_mount_number(fly_mount_t *mnt, const char *path);
int fly_mount_files_count(fly_mount_t *mnt, int mount_number);
char *fly_content_from_path(int mount_number, char *filepath);
int fly_join_path(char *buffer, char *join1, char *join2);

#ifdef HAVE_INOTIFY
int fly_mount_inotify(fly_mount_t *mount, int ifd);
#elif defined HAVE_KQUEUE
int fly_mount_inotify_kevent(fly_event_manager_t *manager, fly_mount_t *mount, void *data, fly_event_handler_t *handler, fly_event_handler_t *end_handler);
int fly_inotify_kevent_event(fly_event_t *event, struct fly_mount_parts_file *pf);
#endif
void fly_parts_file_remove(fly_mount_parts_t *parts, struct fly_mount_parts_file *pf);
struct fly_mount_parts_file *fly_pf_init(fly_mount_parts_t *parts, struct stat *sb);

fly_mount_parts_t *fly_parts_from_wd(int wd, fly_mount_t *mnt);
fly_mount_parts_t *fly_parts_from_fd(int fd, fly_mount_t *mnt);
#ifdef HAVE_INOTIFY
struct fly_mount_parts_file *fly_pf_from_mount(int wd, fly_mount_t *mnt);
struct fly_mount_parts_file *fly_pf_from_wd(int wd, fly_mount_parts_t *parts);
#elif defined HAVE_KQUEUE
struct fly_mount_parts_file *fly_pf_from_mount(int fd, fly_mount_t *mnt);
struct fly_mount_parts_file *fly_pf_from_fd(int fd, fly_mount_parts_t *parts);
#endif
#ifdef HAVE_INOTIFY
int fly_inotify_add_watch(fly_mount_parts_t *parts, char *path, size_t len);
#elif defined HAVE_KQUEUE
int fly_inotify_add_watch(fly_mount_parts_t *parts, fly_event_t *__e);
#endif
#define FLY_INOTIFY_RM_WATCH_PF				(1<<0)
#define FLY_INOTIFY_RM_WATCH_MP				(1<<1)
#define FLY_INOTIFY_RM_WATCH_IGNORED		(1<<2)
#define FLY_INOTIFY_RM_WATCH_DELETED		(1<<3)

#ifdef HAVE_INOTIFY
int fly_inotify_rm_watch(fly_mount_parts_t *parts, char *path, size_t path_len, int mask);
#elif defined HAVE_KQUEUE
int fly_inotify_rm_watch(struct fly_mount_parts_file *pf, int mask);
#endif
int fly_inotify_rmmp(fly_mount_parts_t *parts);

#ifdef HAVE_INOTIFY
#define FLY_INOTIFY_WATCH_FLAG_PF	(IN_MODIFY|IN_ATTRIB)
#define FLY_INOTIFY_WATCH_FLAG_MP	(IN_CREATE|IN_DELETE_SELF|IN_DELETE|IN_MOVE|IN_MOVE_SELF|IN_ONLYDIR)
#elif defined HAVE_KQUEUE
#define FLY_INOTIFY_WATCH_FLAG_MP	(NOTE_DELETE|NOTE_EXTEND|NOTE_LINK)
#define FLY_INOTIFY_WATCH_FLAG_PF	(NOTE_DELETE|NOTE_EXTEND|NOTE_ATTRIB|NOTE_CLOSE_WRITE|NOTE_RENAME)
#endif
#define FLY_NUMBER_OF_INOBUF				(100)
#define is_fly_myself(ie)				((ie)->len == 0)

int fly_parts_file_remove_from_path(fly_mount_parts_t *parts, char *filename);

struct fly_mount_parts_file *fly_pf_from_parts(char *path, size_t path_len, fly_mount_parts_t *parts);
struct fly_mount_parts_file *fly_pf_from_parts_by_fullpath(char *path, fly_mount_parts_t *parts);
void fly_parts_file_add(fly_mount_parts_t *parts, struct fly_mount_parts_file *pf);
const char *fly_index_path(void);
#include "uri.h"
int fly_found_content_from_path(fly_mount_t *mnt, fly_uri_t *uri, struct fly_mount_parts_file **res);
#include "event.h"
int fly_send_from_pf(fly_event_t *e, int c_sockfd, struct fly_mount_parts_file *pf, off_t *offset, size_t count);

static inline void fly_mount_index_parts_file(struct fly_mount_parts_file *pf)
{
	pf->parts->mount->index = pf;
}

static inline bool fly_have_mount_index(struct fly_mount *mount)
{
	return mount->index != NULL ? true : false;
}

#ifdef DEBUG
__fly_unused static struct fly_mount_parts_file *fly_pf_debug(struct fly_bllist *__b)
{
	return (struct fly_mount_parts_file *) fly_bllist_data(__b, struct fly_mount_parts_file, blelem);
}
__fly_unused static struct fly_mount_parts *fly_parts_debug(struct fly_bllist *__b)
{
	return (struct fly_mount_parts *) fly_bllist_data(__b, struct fly_mount_parts, mbelem);
}
#endif

#endif
