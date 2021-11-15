
#define _GNU_SOURCE
#include <unistd.h>
#include "worker.h"
#include "fsignal.h"
#include "cache.h"
#include "response.h"
#include "ssl.h"
#include "context.h"
#include "conf.h"
#include "event.h"

__fly_static int fly_wainting_for_connection_event(fly_event_manager_t *manager, fly_sockinfo_t *sockinfo);
#define FLY_WORKER_SIGNAL_EVENT_INVALID_MANAGER			-1
#define FLY_WORKER_SIGNAL_EVENT_REFRESH_SIGNAL_ERROR	-2
#define FLY_WORKER_SIGNAL_EVENT_SIGNAL_FILLSET_ERROR	-3
#define FLY_WORKER_SIGNAL_EVENT_SIGNAL_ERROR			-4
#define FLY_WORKER_SIGNAL_EVENT_SIGADDSET_ERROR			-5
#define FLY_WORKER_SIGNAL_EVENT_INIT_ERROR				-6
#define FLY_WORKER_SIGNAL_EVENT_SIGNAL_REGISTER_ERROR	-7
__fly_static int __fly_worker_signal_event(fly_worker_t *worker, fly_event_manager_t *manager, fly_context_t *ctx);
__fly_static int __fly_worker_signal_handler(fly_event_t *e);
__fly_static int __fly_add_worker_sigs(fly_context_t *ctx, int num, fly_sighand_t *handler);
__fly_static void FLY_SIGNAL_MODF_HANDLER(__unused fly_context_t *ctx, __unused struct signalfd_siginfo *info);
__fly_static void FLY_SIGNAL_ADDF_HANDLER(__unused fly_context_t *ctx, __unused struct signalfd_siginfo *info);
__fly_static void FLY_SIGNAL_DELF_HANDLER(__unused fly_context_t *ctx, __unused struct signalfd_siginfo *info);
__fly_static void FLY_SIGNAL_UMOU_HANDLER(__unused fly_context_t *ctx, __unused struct signalfd_siginfo *info);
#define FLY_WORKER_OPEN_FILE_SUCCESS				0
#define FLY_WORKER_OPEN_FILE_CTX_ERROR				-2
#define FLY_WORKER_OPEN_FILE_SETTING_DATE_ERROR		-3
#define FLY_WORKER_OPEN_FILE_ENCODE_ERROR			-4
#define FLY_WORKER_OPEN_FILE_ENCODE_UNKNOWN_RETURN	-5
__fly_static int __fly_worker_open_file(fly_context_t *ctx);
#define FLY_WORKER_OPEN_DEFAULT_CONTENT_NOCONTENT				0
#define FLY_WORKER_OPEN_DEFAULT_CONTENT_NODIR					1
#define FLY_WORKER_OPEN_DEFAULT_CONTENT_SUCCESS					2
#define FLY_WORKER_OPEN_DEFAULT_CONTENT_SOLVPATH_ERROR			-1
#define FLY_WORKER_OPEN_DEFAULT_CONTENT_FRC_INIT_ERROR			-2
#define FLY_WORKER_OPEN_DEFAULT_CONTENT_INVALID_FILE			-3
#define FLY_WORKER_OPEN_DEFAULT_CONTENT_ENCODE_ERROR			-4
#define FLY_WORKER_OPEN_DEFAULT_CONTENT_ENCODE_UNKNOWN_RETURN	-5
__fly_static int __fly_worker_open_default_content(fly_context_t *ctx);


#define FLY_WORKER_SIG_COUNT				(sizeof(fly_worker_signals)/sizeof(fly_signal_t))

/*
 *  worker process signal info.
 */
static fly_signal_t fly_worker_signals[] = {
	FLY_SIGNAL_SETTING(SIGINT,	NULL),
	FLY_SIGNAL_SETTING(SIGTERM, NULL),
	FLY_SIGNAL_SETTING(SIGPIPE, FLY_SIG_IGN),
};

/*
 *	 alloc resource:
 *	 pool_manager, struct fly_worker, struct fly_context
 */
struct fly_worker *fly_worker_init(fly_context_t *mcontext)
{
	struct fly_pool_manager *__pm;
	fly_worker_t *__w;

	__pm = fly_pool_manager_init();
	if (fly_unlikely_null(__pm))
		goto pm_error;

	__w = fly_malloc(sizeof(fly_worker_t));
	if (fly_unlikely_null(__w))
		goto w_error;

	__w->context = mcontext;
	__w->context->event_pool = NULL;
	__w->context->pool_manager = __pm;

	/* move to master pool manager to worker pool manager */
	__w->context->pool->manager = __pm;
	__w->context->misc_pool->manager = __pm;
	fly_bllist_add_tail(&__pm->pools, &__w->context->pool->pbelem);
	fly_bllist_add_tail(&__pm->pools, &__w->context->misc_pool->pbelem);
	__w->pid = getpid();
	__w->ppid = getppid();
	__w->master = NULL;
	__w->start = time(NULL);
	__w->pool_manager = __pm;
	__w->event_manager = NULL;
	fly_bllist_init(&__w->signals);

	return __w;

w_error:
	fly_pool_manager_release(__pm);
pm_error:
	return NULL;
}

void fly_worker_release(fly_worker_t *worker)
{
	assert(worker != NULL);

	if (worker->event_manager)
		fly_event_manager_release(worker->event_manager);

	/* context is self delete */
	fly_context_release(worker->context);

	fly_pool_manager_release(worker->pool_manager);
	fly_free(worker);
}

__fly_static int __fly_add_worker_sigs(fly_context_t *ctx, int num, fly_sighand_t *handler)
{
	fly_worker_t *__w;
	fly_signal_t *__nf;

	__w = (fly_worker_t *) ctx->data;
	__nf = fly_pballoc(ctx->pool, sizeof(struct fly_signal));
	if (fly_unlikely_null(__nf))
		return -1;
	__nf->number = num;
	__nf->handler = handler;
	fly_bllist_add_tail(&__w->signals, &__nf->blelem);
	return 0;
}

__fly_static void __fly_modupdate(fly_mount_parts_t *parts)
{
	if (parts->file_count == 0)
		return;

	char rpath[FLY_PATH_MAX];

	struct fly_bllist *__b;
	struct fly_mount_parts_file *__f;

	fly_for_each_bllist(__b, &parts->files){
		__f = fly_bllist_data(__b, struct fly_mount_parts_file, blelem);
		if (fly_join_path(rpath, parts->mount_path, __f->filename) == -1)
			return;
		if (fly_hash_update_from_parts_file_path(rpath, __f) == -1)
			FLY_EMERGENCY_ERROR(
				"modify file error in worker."
			);
	}
	return;
}

static void __fly_path_cpy(char *dist, char *src, const char *mount_point)
{
	/* ignore up to mount point */
	while (*src++ == *mount_point++)
		;

	if (*src == '/')	src++;
	while (*src)
		*dist++ = *src++;
}

__fly_static int __fly_work_add_nftw(fly_mount_parts_t *parts, char *path, const char *mount_point)
{
	DIR *__pathd;
	struct dirent *__ent;
	char __path[FLY_PATH_MAX];
	struct fly_mount_parts_file *__pf;
	struct stat sb;

	__pathd = opendir(path);
	if (__pathd == NULL)
		return -1;

	while((__ent=readdir(__pathd)) != NULL){
		if (strcmp(__ent->d_name, ".") == 0 ||
				strcmp(__ent->d_name, "..") == 0)
			continue;

		if (fly_join_path(__path, parts->mount_path, __ent->d_name) == -1)
			goto error;

		if (fly_isdir(__path) == 1){
			if (__fly_work_add_nftw(parts, __path, mount_point) == -1)
				goto error;
			continue;
		}

		__pf = fly_pf_from_parts_by_fullpath(__path, parts);
		/* already register in parts */
		if (__pf)
			continue;

		/* new file regsiter in parts */
		if (stat(__path, &sb) == -1)
			goto error;

		__pf = fly_pf_init(parts, &sb);
		if (fly_unlikely_null(__pf))
			goto error;

		__pf->fd = open(__path, O_RDONLY);
		if (__pf->fd == -1)
			goto error;
		__fly_path_cpy(__pf->filename, __path, mount_point);
		__pf->filename_len = strlen(__pf->filename);
		__pf->parts = parts;
		__pf->infd = parts->infd;
		__pf->mime_type = fly_mime_type_from_path_name(__path);
		if (fly_hash_from_parts_file_path(__path, __pf) == -1)
			goto error;

		fly_parts_file_add(parts, __pf);
		parts->mount->file_count++;
		__pf->rbnode = fly_rb_tree_insert(parts->mount->rbtree, (void *) __pf, (void *) __pf->filename, &__pf->rbnode, (void *) __pf->filename_len);
	}
	return closedir(__pathd);
error:
	closedir(__pathd);
	return -1;
}

__fly_static int __fly_work_del_nftw(fly_mount_parts_t *parts, __unused char *path, const char *mount_point)
{
	if (parts->file_count == 0)
		return -1;

	char __path[FLY_PATH_MAX];
	struct fly_mount_parts_file *__pf;
	struct fly_bllist *__b;

	fly_for_each_bllist(__b, &parts->files){
		__pf = fly_bllist_data(__b, struct fly_mount_parts_file, blelem);
		if (fly_join_path(__path, (char *) mount_point, __pf->filename) == -1 \
				&& errno == ENOENT){
			if (__pf->fd != -1)
				if (close(__pf->fd) == -1)
					return -1;

			fly_parts_file_remove(parts, __pf);
			parts->mount->file_count--;
		}
	}

	return 0;
}

__fly_static int __fly_work_unmount(fly_mount_parts_t *parts)
{
	struct fly_mount_parts_file *__pf;
	struct fly_bllist *__b;

	/* close all mount file */
	fly_for_each_bllist(__b, &parts->files){
		__pf = fly_bllist_data(__b, struct fly_mount_parts_file, blelem);
		if (__pf->fd != -1 && close(__pf->fd) == -1)
			return -1;

		fly_parts_file_remove(parts, __pf);
		parts->mount->file_count--;
	}
	return fly_unmount(parts->mount, parts->mount_path);
}

__fly_static void __fly_add_file_by_signal(fly_mount_parts_t *parts)
{
	__fly_work_add_nftw(parts, parts->mount_path, parts->mount_path);
}

__fly_static void __fly_del_file_by_signal(fly_mount_parts_t *parts)
{
	__fly_work_del_nftw(parts, parts->mount_path, parts->mount_path);
}

__fly_static void __fly_unmount_by_signal(fly_mount_parts_t *parts)
{
	__fly_work_unmount(parts);
}

__fly_static int __fly_signal_handler(fly_context_t *ctx, int mount_number, void (*handler)(fly_mount_parts_t *))
{
	struct fly_bllist *__b;
	struct fly_mount_parts *parts;

	fly_for_each_bllist(__b, &ctx->mount->parts){
		parts = fly_bllist_data(__b, struct fly_mount_parts, mbelem);
		if (parts->mount_number == mount_number){
			handler(parts);
			return 0;
		}
	}
	return 0;
}

__fly_static void FLY_SIGNAL_MODF_HANDLER(__unused fly_context_t *ctx, __unused struct signalfd_siginfo *info)
{
	int mount_number;
	if (!ctx->mount)
		return;

	mount_number = info->ssi_int;
	__fly_signal_handler(ctx, mount_number, __fly_modupdate);
}

__fly_static void FLY_SIGNAL_ADDF_HANDLER(__unused fly_context_t *ctx, __unused struct signalfd_siginfo *info)
{
	int mount_number;

	if (!ctx->mount)
		return;

	mount_number = info->ssi_int;
	__fly_signal_handler(ctx, mount_number, __fly_add_file_by_signal);
}

__fly_static void FLY_SIGNAL_DELF_HANDLER(__unused fly_context_t *ctx, __unused struct signalfd_siginfo *info)
{
	int mount_number;

	if (!ctx->mount)
		return;

	mount_number = info->ssi_int;
	__fly_signal_handler(ctx, mount_number, __fly_del_file_by_signal);
}

__fly_static void FLY_SIGNAL_UMOU_HANDLER(__unused fly_context_t *ctx, __unused struct signalfd_siginfo *info)
{
	int mount_number;

	if (!ctx->mount)
		return;

	mount_number = info->ssi_int;
	__fly_signal_handler(ctx, mount_number, __fly_unmount_by_signal);
}

__noreturn void fly_worker_signal_default_handler(fly_worker_t *worker, fly_context_t *ctx __unused, struct signalfd_siginfo *si __unused)
{
	fly_worker_release(worker);
	exit(0);
}

__fly_static int __fly_wsignal_handle(fly_worker_t *worker, fly_context_t *ctx, struct signalfd_siginfo *info)
{
	struct fly_signal *__s;
	struct fly_bllist *__b;

	fly_for_each_bllist(__b, &worker->signals){
		__s = fly_bllist_data(__b, struct fly_signal, blelem);
		if (__s->number == (fly_signum_t) info->ssi_signo){
			if (__s->handler)
				__s->handler(ctx, info);
			else
				fly_worker_signal_default_handler(worker, ctx, info);

			return 0;
		}
	}
	return 0;
}

__fly_static int __fly_worker_signal_handler(fly_event_t *e)
{
	struct signalfd_siginfo info;
	ssize_t res;

	while(true){
		res = read(e->fd, (void *) &info,sizeof(struct signalfd_siginfo));
		if (res == -1){
			if (errno == EAGAIN || errno == EWOULDBLOCK)
				break;
			else
				return -1;
		}
		if (__fly_wsignal_handle((fly_worker_t *) e->event_data, e->manager->ctx, &info) == -1)
			return -1;
	}

	return 0;
}

__fly_static int __fly_worker_rtsig_added(fly_context_t *ctx, sigset_t *sset)
{
	FLY_RTSIGSET(MODF, sset);
	FLY_RTSIGSET(ADDF, sset);
	FLY_RTSIGSET(DELF, sset);
	FLY_RTSIGSET(UMOU, sset);
	return 0;
}

__fly_static int __fly_worker_signal_event(fly_worker_t *worker, fly_event_manager_t *manager, fly_context_t *ctx)
{
	sigset_t sset;
	int sigfd;
	fly_event_t *e;

	if (!manager ||  !manager->pool || !ctx)
		return FLY_WORKER_SIGNAL_EVENT_INVALID_MANAGER;

	if (fly_refresh_signal() == -1)
		return FLY_WORKER_SIGNAL_EVENT_REFRESH_SIGNAL_ERROR;
	if (sigfillset(&sset) == -1)
		return FLY_WORKER_SIGNAL_EVENT_SIGNAL_FILLSET_ERROR;

	for (int i=0; i<(int) FLY_WORKER_SIG_COUNT; i++){
		if (fly_worker_signals[i].handler == FLY_SIG_IGN){
			if (signal(fly_worker_signals[i].number, SIG_IGN) == SIG_ERR)
				return FLY_WORKER_SIGNAL_EVENT_SIGNAL_ERROR;
			continue;
		}

		if (sigaddset(&sset, fly_worker_signals[i].number) == -1)
			return FLY_WORKER_SIGNAL_EVENT_SIGADDSET_ERROR;
		if (__fly_add_worker_sigs(ctx, fly_worker_signals[i].number, fly_worker_signals[i].handler) == -1)
			return -1;
	}

	if (__fly_worker_rtsig_added(ctx, &sset) == -1)
		return -1;

	sigfd = fly_signal_register(&sset);
	if (sigfd == -1)
		return FLY_WORKER_SIGNAL_EVENT_SIGNAL_REGISTER_ERROR;

	e = fly_event_init(manager);
	if (e == NULL)
		return FLY_WORKER_SIGNAL_EVENT_INIT_ERROR;

	e->fd = sigfd;
	e->read_or_write = FLY_READ;
	e->tflag = FLY_INFINITY;
	e->eflag = 0;
	e->flag = FLY_PERSISTENT;
	e->event_fase = NULL;
	e->event_state = NULL;
	e->expired = false;
	e->available = false;
	e->handler = __fly_worker_signal_handler;
	e->event_data = (void *) worker;

	fly_time_null(e->timeout);
	fly_event_file_type(e, SIGNAL);
	return fly_event_register(e);
}

/*
 * this function is called after fork from master process.
 * @params:
 *		ctx:  passed from master process. include fly context info.
 *		data: custom data.
 */
__direct_log __noreturn void fly_worker_process(fly_context_t *ctx, __unused void *data)
{
	fly_worker_t *worker;
	fly_event_manager_t *manager;

	worker = (fly_worker_t *) data;
	if (!ctx)
		FLY_EMERGENCY_ERROR(
			"worker context is invalid(null context)."
		);
	ctx->data = (void *) worker;
	fly_errsys_init(ctx);

	switch (__fly_worker_open_file(ctx)){
	case FLY_WORKER_OPEN_FILE_SUCCESS:
		break;
	case FLY_WORKER_OPEN_FILE_CTX_ERROR:
		FLY_EMERGENCY_ERROR(
			"worker opening file error. \
			worker context is invalid. (%s: %s)",
			__FILE__,
			__LINE__
		);
		break;
	case FLY_WORKER_OPEN_FILE_SETTING_DATE_ERROR:
		FLY_EMERGENCY_ERROR(
			"worker opening file error. \
			occurred error when solving time. (%s: %s)",
			__FILE__,
			__LINE__
		);
		break;
	case FLY_WORKER_OPEN_FILE_ENCODE_ERROR:
		FLY_EMERGENCY_ERROR(
			"worker opening file error. \
			occurred error when encoding opening file. (%s: %s)",
			__FILE__,
			__LINE__
		);
		break;
	case FLY_WORKER_OPEN_FILE_ENCODE_UNKNOWN_RETURN:
		FLY_EMERGENCY_ERROR(
			"worker opening file error. \
			unknown return value in encoding opening file. (%s: %s)",
			__FILE__,
			__LINE__
		);
		break;
	default:
		FLY_EMERGENCY_ERROR(
			"worker opening file error. \
			unknown return value. (%s: %s)",
			__FILE__,
			__LINE__
		);
	}

	switch(__fly_worker_open_default_content(ctx)){
	case FLY_WORKER_OPEN_DEFAULT_CONTENT_NOCONTENT:
		break;
	case FLY_WORKER_OPEN_DEFAULT_CONTENT_NODIR:
		break;
	case FLY_WORKER_OPEN_DEFAULT_CONTENT_SUCCESS:
		break;
	case FLY_WORKER_OPEN_DEFAULT_CONTENT_SOLVPATH_ERROR:
		FLY_EMERGENCY_ERROR(
			"worker opening default content error.	\
			solving path error in opening worker default content.",
			__FILE__,
			__LINE__
		);
		break;
	case FLY_WORKER_OPEN_DEFAULT_CONTENT_FRC_INIT_ERROR:
		FLY_EMERGENCY_ERROR(
			"worker opening default content error.	\
			frc init error in opening worker default content.",
			__FILE__,
			__LINE__
		);
		break;
	case FLY_WORKER_OPEN_DEFAULT_CONTENT_INVALID_FILE:
		FLY_EMERGENCY_ERROR(
			"worker opening default content error.	\
			found invalid file in opening worker default content.",
			__FILE__,
			__LINE__
		);
		break;
	case FLY_WORKER_OPEN_DEFAULT_CONTENT_ENCODE_ERROR:
		FLY_EMERGENCY_ERROR(
			"worker opening default content error.	\
			occurred error when encoding default content. (%s: %s)",
			__FILE__,
			__LINE__
		);
		break;
	case FLY_WORKER_OPEN_DEFAULT_CONTENT_ENCODE_UNKNOWN_RETURN:
		FLY_EMERGENCY_ERROR(
			"worker opening default content error.	\
			unknown return value in encoding default content. (%s: %s)",
			__FILE__,
			__LINE__
		);
		break;
	default:
		FLY_EMERGENCY_ERROR(
			"worker open default content error.	\
			unknown return value in opening default content. (%s: %s)",
			__FILE__,
			__LINE__
		);
	}

	manager = fly_event_manager_init(ctx);
	if (manager == NULL)
		FLY_EMERGENCY_ERROR(
			"worker event manager init error. (%s: %s)",
			__FILE__,
			__LINE__
		);

	worker->event_manager = manager;
	/* initial event */
	/* signal setting */
	if (__fly_worker_signal_event(worker, manager, ctx) == -1)
		FLY_EMERGENCY_ERROR(
			"initialize worker signal error. (%s: %s)",
			__FILE__,
			__LINE__
		);

	/* make socket for each socket info */
	if (fly_wainting_for_connection_event(manager, ctx->listen_sock) == -1)
		FLY_EMERGENCY_ERROR(
			"fail to register listen socket event. (%s: %s)",
			__FILE__,
			__LINE__
		);

	/* log event start here */
	switch(fly_event_handler(manager)){
	case FLY_EVENT_HANDLER_INVALID_MANAGER:
		FLY_EMERGENCY_ERROR(
			"event handle error. \
			event manager is invalid. (%s: %s)",
			__FILE__,
			__LINE__
		);
		break;
	case FLY_EVENT_HANDLER_EPOLL_ERROR:
		FLY_EMERGENCY_ERROR(
			"event handle error. \
			epoll was broken. (%s: %s)",
			__FILE__,
			__LINE__
		);
		break;
	case FLY_EVENT_HANDLER_EXPIRED_EVENT_ERROR:
		FLY_EMERGENCY_ERROR(
			"event handle error. \
			occurred error in expired event handler. (%s: %s)",
			__FILE__,
			__LINE__
		);
		break;
	default:
		FLY_NOT_COME_HERE
	}

	/* will not come here. */
	FLY_NOT_COME_HERE
}

__fly_static int fly_wainting_for_connection_event(fly_event_manager_t *manager, fly_sockinfo_t *sockinfo)
{
	fly_event_t *e;

	e = fly_event_init(manager);
	if (fly_unlikely_null(e))
		return -1;

	if (sockinfo->flag & FLY_SOCKINFO_SSL)
		/* SSL setting */
		fly_listen_socket_ssl_setting(manager->ctx, sockinfo);

	e->fd = sockinfo->fd;
	e->read_or_write = FLY_READ;
	FLY_EVENT_HANDLER(e, fly_accept_listen_socket_handler);
	e->flag = FLY_PERSISTENT;
	e->tflag = FLY_INFINITY;
	e->eflag = 0;
	fly_time_null(e->timeout);
	e->event_data = sockinfo;
	e->expired = false;
	e->available = false;
	fly_event_socket(e);

	return fly_event_register(e);
}

__fly_static int __fly_worker_open_file(fly_context_t *ctx)
{
	if ((!ctx->mount || ctx->mount->mount_count == 0) && \
			(!ctx->route_reg || ctx->route_reg->regcount == 0))
		return FLY_WORKER_OPEN_FILE_CTX_ERROR;

	char rpath[FLY_PATH_MAX];
	fly_mount_parts_t *__p;
	struct fly_mount_parts_file *__pf;
	struct fly_bllist *__b, *__pfb, *__n;

	fly_for_each_bllist(__b, &ctx->mount->parts){
		__p = fly_bllist_data(__b, fly_mount_parts_t, mbelem);
		if (__p->file_count == 0)
			continue;

		for (__pfb=__p->files.next; __pfb!=&__p->files; __pfb=__n){
			__n = __pfb->next;
			__pf = fly_bllist_data(__pfb, struct fly_mount_parts_file, blelem);
			if (__pf->dir){
				fly_parts_file_remove(__p, __pf);
			}else{
				if (fly_join_path(rpath, __p->mount_path, __pf->filename) == -1)
					continue;

				__pf->fd = open(rpath, O_RDONLY);
				/* if index, setting */
				const char *index_path = fly_index_path();
				if (strncmp(__pf->filename, index_path, strlen(index_path)) == 0)
					fly_mount_index_parts_file(__pf);

				if (fly_imt_fixdate(__pf->last_modified, FLY_DATE_LENGTH, &__pf->fs.st_mtime) == -1)
					return FLY_WORKER_OPEN_FILE_SETTING_DATE_ERROR;
				/* pre encode */
				if (fly_over_encoding_threshold(ctx, (size_t) __pf->fs.st_size)){
					struct fly_de *__de;
					int res;

					__de = fly_de_init(__p->pool);
					__de->type = FLY_DE_FROM_PATH;
					__de->fd = __pf->fd;
					__de->offset = 0;
					__de->count = __pf->fs.st_size;
					__de->etype = fly_encoding_from_type(__pf->encode_type);
					if (fly_response_content_max_length() >= __pf->fs.st_size){
						size_t __max;

						__max = fly_response_content_max_length();
						__de->encbuf = fly_buffer_init(__de->pool, FLY_WORKER_ENCBUF_INIT_LEN, FLY_WORKER_ENCBUF_CHAIN_MAX(__max), FLY_WORKER_ENCBUF_PER_LEN);
						__de->decbuf = fly_buffer_init(__de->pool, FLY_WORKER_DECBUF_INIT_LEN, FLY_WORKER_DECBUF_CHAIN_MAX, FLY_WORKER_DECBUF_PER_LEN);
						__de->encbuflen = FLY_WORKER_ENCBUF_INIT_LEN;
						__de->decbuflen = FLY_WORKER_DECBUF_INIT_LEN;
#ifdef DEBUG
						assert(__max < (size_t) (__de->encbuf->per_len*__de->encbuf->chain_max));
#endif
						res = __de->etype->encode(__de);
						switch(res){
						case FLY_ENCODE_SUCCESS:
							break;
						case FLY_ENCODE_ERROR:
							return FLY_WORKER_OPEN_FILE_ENCODE_ERROR;
						case FLY_ENCODE_SEEK_ERROR:
							return FLY_WORKER_OPEN_FILE_ENCODE_ERROR;
						case FLY_ENCODE_TYPE_ERROR:
							return FLY_WORKER_OPEN_FILE_ENCODE_ERROR;
						case FLY_ENCODE_READ_ERROR:
							return FLY_WORKER_OPEN_FILE_ENCODE_ERROR;
						case FLY_ENCODE_BUFFER_ERROR:
							__de->overflow = true;
							break;
						default:
							return FLY_WORKER_OPEN_FILE_ENCODE_UNKNOWN_RETURN;
						}
					}else{
						__de->overflow = true;
						__pf->overflow = true;
					}

					__pf->de = __de;
					__pf->encoded = true;
				}
			}
		}
	}
	return FLY_WORKER_OPEN_FILE_SUCCESS;
}

__fly_static void __fly_add_rcbs(fly_context_t *ctx, fly_rcbs_t *__r)
{
	fly_bllist_add_tail(&ctx->rcbs, &__r->blelem);
}

/* open worker default content by status code */
extern fly_status_code responses[];
__fly_static int __fly_worker_open_default_content(fly_context_t *ctx)
{
	const char *__defpath;
	struct stat sb;

	__defpath = fly_default_content_path();
	if (__defpath == NULL)
		return FLY_WORKER_OPEN_DEFAULT_CONTENT_NOCONTENT;

	if (stat(__defpath, &sb) == -1 || !S_ISDIR(sb.st_mode))
		return FLY_WORKER_OPEN_DEFAULT_CONTENT_NODIR;

	for (fly_status_code *__res=responses; __res->status_code>0; __res++){
		char __tmp[FLY_PATH_MAX], *__tmpptr;

		__tmpptr = __tmp;
		memset(__tmp, '\0', FLY_PATH_MAX);
		if (realpath(__defpath, __tmp) == NULL)
			return FLY_WORKER_OPEN_DEFAULT_CONTENT_SOLVPATH_ERROR;

		__tmpptr += strlen(__tmp);
		*__tmpptr++ = '/';
		sprintf(__tmpptr, "%d.html", __res->status_code);
		if (access(__tmp, R_OK) == -1)
			continue;

		/* there is a default content path */
		struct fly_response_content_by_stcode *__frc;

		__frc = fly_rcbs_init(ctx);
		if (fly_unlikely_null(__frc))
			return FLY_WORKER_OPEN_DEFAULT_CONTENT_FRC_INIT_ERROR;

		__frc->status_code = __res->type;
		memcpy(__frc->content_path, __tmp, FLY_PATH_MAX);
		__frc->mime = fly_mime_type_from_path_name(__frc->content_path);
		__frc->fd = open(__frc->content_path, O_RDONLY);
		if (__frc->fd == -1)
			continue;

		if (fstat(__frc->fd, &__frc->fs) == -1)
			return FLY_WORKER_OPEN_DEFAULT_CONTENT_INVALID_FILE;

		if (fly_over_encoding_threshold(ctx, (size_t) __frc->fs.st_size)){
			struct fly_de *__de;

			__de = fly_de_init(ctx->pool);
			__de->type = FLY_DE_FROM_PATH;
			__de->fd = __frc->fd;
			__de->offset = 0;
			__de->count = __frc->fs.st_size;
			__de->etype = fly_encoding_from_type(__frc->encode_type);
			if (fly_response_content_max_length() >= __frc->fs.st_size){
				size_t __max;
				int res;

				__max = fly_response_content_max_length();
				__de->encbuf = fly_buffer_init(__de->pool, FLY_WORKER_ENCBUF_INIT_LEN, FLY_WORKER_ENCBUF_CHAIN_MAX(__max), FLY_WORKER_ENCBUF_PER_LEN);
				__de->decbuf = fly_buffer_init(__de->pool, FLY_WORKER_DECBUF_INIT_LEN, FLY_WORKER_DECBUF_CHAIN_MAX, FLY_WORKER_DECBUF_PER_LEN);
				__de->encbuflen = FLY_WORKER_ENCBUF_INIT_LEN;
				__de->decbuflen = FLY_WORKER_DECBUF_INIT_LEN;
#ifdef DEBUG
				assert(__max < (size_t) (__de->encbuf->per_len*__de->encbuf->chain_max));
#endif
				res = __de->etype->encode(__de);
				switch(res){
				case FLY_ENCODE_SUCCESS:
					break;
				case FLY_ENCODE_ERROR:
					return FLY_WORKER_OPEN_DEFAULT_CONTENT_ENCODE_ERROR;
				case FLY_ENCODE_SEEK_ERROR:
					return FLY_WORKER_OPEN_DEFAULT_CONTENT_ENCODE_ERROR;
				case FLY_ENCODE_TYPE_ERROR:
					return FLY_WORKER_OPEN_DEFAULT_CONTENT_ENCODE_ERROR;
				case FLY_ENCODE_READ_ERROR:
					return FLY_WORKER_OPEN_DEFAULT_CONTENT_ENCODE_ERROR;
				case FLY_ENCODE_BUFFER_ERROR:
					__de->overflow = true;
					break;
				default:
					return FLY_WORKER_OPEN_DEFAULT_CONTENT_ENCODE_UNKNOWN_RETURN;
				}
			}else{
				__de->overflow = true;
			}

			__frc->de = __de;
			__frc->encoded = true;
		}

		__fly_add_rcbs(ctx, __frc);
	}
	return FLY_WORKER_OPEN_DEFAULT_CONTENT_SUCCESS;
}

const char *fly_default_content_path(void)
{
	return (const char *) fly_config_value_str(FLY_DEFAULT_CONTENT_PATH);
}
