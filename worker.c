
#define _GNU_SOURCE
#include <unistd.h>
#include "worker.h"
#include "fsignal.h"
#include "cache.h"
#include "response.h"
#include "ssl.h"
#include "context.h"
#include "config.h"

__fly_static int __fly_listen_socket_event(fly_event_manager_t *manager, fly_sockinfo_t *sockinfo);
__fly_static int __fly_listen_socket_handler(struct fly_event *);
__fly_static fly_connect_t *__fly_connected(fly_sock_t fd, fly_sock_t cfd, fly_event_t *e, struct sockaddr *addr, socklen_t addrlen);
__fly_static int __fly_worker_signal_event(fly_worker_t *worker, fly_event_manager_t *manager, fly_context_t *ctx);
__fly_static int __fly_worker_signal_handler(fly_event_t *e);
__fly_static int __fly_add_worker_sigs(fly_context_t *ctx, int num, fly_sighand_t *handler);
__fly_static void FLY_SIGNAL_MODF_HANDLER(__unused fly_context_t *ctx, __unused struct signalfd_siginfo *info);
__fly_static void FLY_SIGNAL_ADDF_HANDLER(__unused fly_context_t *ctx, __unused struct signalfd_siginfo *info);
__fly_static void FLY_SIGNAL_DELF_HANDLER(__unused fly_context_t *ctx, __unused struct signalfd_siginfo *info);
__fly_static void FLY_SIGNAL_UMOU_HANDLER(__unused fly_context_t *ctx, __unused struct signalfd_siginfo *info);
__fly_static int __fly_worker_open_file(fly_context_t *ctx);
__fly_static int __fly_worker_open_default_content(fly_context_t *ctx);


#define FLY_WORKER_SIG_COUNT				(sizeof(fly_worker_signals)/sizeof(fly_signal_t))
/*
 *  worker process signal info.
 */
static struct fly_signal *fly_worker_sigptr = NULL;
static fly_signal_t fly_worker_signals[] = {
	{SIGINT,			NULL, NULL},
	{SIGTERM,			NULL, NULL},
	{SIGPIPE,			FLY_SIG_IGN, NULL},
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
	fly_signal_t *__nf;

	__nf = fly_pballoc(ctx->pool, sizeof(struct fly_signal));
	if (fly_unlikely_null(__nf))
		return -1;
	__nf->number = num;
	__nf->handler = handler;
	__nf->next = NULL;

	if (fly_worker_sigptr == NULL)
		fly_worker_sigptr = __nf;
	else{
		fly_signal_t *__f;
		for (__f=fly_worker_sigptr; __f->next; __f=__f->next)
			;

		__f->next = __nf;
	}
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
				FLY_EMERGENCY_STATUS_MODF,
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
		__pf->parts = parts;
		__pf->infd = parts->infd;
		__pf->mime_type = fly_mime_type_from_path_name(__path);
		if (fly_hash_from_parts_file_path(__path, __pf) == -1)
			goto error;

		fly_parts_file_add(parts, __pf);
		parts->mount->file_count++;
		__pf->rbnode = fly_rb_tree_insert(parts->mount->rbtree, (void *) __pf, (void *) __pf->filename, &__pf->rbnode);
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
	fly_signal_t *__s;
	for (__s=fly_worker_sigptr; __s; __s=__s->next){
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
		return -1;

	if (fly_refresh_signal() == -1)
		return -1;
	if (sigfillset(&sset) == -1)
		return -1;

	for (int i=0; i<(int) FLY_WORKER_SIG_COUNT; i++){
		if (fly_worker_signals[i].handler == FLY_SIG_IGN){
			if (signal(fly_worker_signals[i].number, SIG_IGN) == SIG_ERR)
				return -1;
			continue;
		}

		if (sigaddset(&sset, fly_worker_signals[i].number) == -1)
			return -1;
		if (__fly_add_worker_sigs(ctx, fly_worker_signals[i].number, fly_worker_signals[i].handler) == -1)
			return -1;
	}

	if (__fly_worker_rtsig_added(ctx, &sset) == -1)
		return -1;

	sigfd = fly_signal_register(&sset);
	if (sigfd == -1)
		return -1;

	e = fly_event_init(manager);
	if (e == NULL)
		return -1;

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
			FLY_EMERGENCY_STATUS_READY,
			"invalid context(null context)."
		);

	if (__fly_worker_open_file(ctx) == -1)
		FLY_EMERGENCY_ERROR(
			FLY_EMERGENCY_STATUS_READY,
			"worker open file error."
		);

	if (__fly_worker_open_default_content(ctx) == -1)
		FLY_EMERGENCY_ERROR(
			FLY_EMERGENCY_STATUS_READY,
			"worker open default content error."
		);

	manager = fly_event_manager_init(ctx);
	if (manager == NULL)
		FLY_EMERGENCY_ERROR(
			FLY_EMERGENCY_STATUS_READY,
			"initialize event manager error."
		);

	worker->event_manager = manager;
	/* initial event */
	/* signal setting */
	if (__fly_worker_signal_event(worker, manager, ctx) == -1)
		FLY_EMERGENCY_ERROR(
			FLY_EMERGENCY_STATUS_READY,
			"initialize worker signal error."
		);

	/* make socket for each socket info */
	if (__fly_listen_socket_event(manager, ctx->listen_sock) == -1)
		FLY_EMERGENCY_ERROR(
			FLY_EMERGENCY_STATUS_PROCS,
			"fail to register listen socket event."
		);

	/* log event start here */
	if (fly_event_handler(manager) == -1)
		FLY_EMERGENCY_ERROR(
			FLY_EMERGENCY_STATUS_PROCS,
			"event handle error."
		);

	/* will not come here. */
	FLY_NOT_COME_HERE
}

__fly_static int __fly_listen_socket_event(fly_event_manager_t *manager, fly_sockinfo_t *sockinfo)
{
	fly_event_t *e;

	e = fly_event_init(manager);
	if (fly_unlikely_null(e))
		return -1;

	e->fd = sockinfo->fd;
	e->read_or_write = FLY_READ;
	if (sockinfo->flag & FLY_SOCKINFO_SSL){
		/* SSL setting */
		fly_listen_socket_ssl_setting(manager->ctx, sockinfo);
		/* tcp+ssl/tls connection */
		FLY_EVENT_HANDLER(e, fly_listen_socket_ssl_handler);
	}else{
		/* tcp connection */
		FLY_EVENT_HANDLER(e, __fly_listen_socket_handler);
	}
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

__fly_static int __fly_listen_socket_handler(struct fly_event *event)
{
	fly_sock_t conn_sock;
	fly_sock_t listen_sock = event->fd;
	struct sockaddr_storage addr;
	socklen_t addrlen;
	int flag;
	fly_event_t *ne;

	addrlen = sizeof(struct sockaddr_storage);
	flag = SOCK_NONBLOCK | SOCK_CLOEXEC;
	conn_sock = accept4(listen_sock, (struct sockaddr *) &addr, &addrlen, flag);
	if (conn_sock == -1)
		return -1;

	/* create new connected socket event. */
	ne = fly_event_init(event->manager);
	if (ne == NULL)
		return -1;
	ne->fd = conn_sock;
	ne->read_or_write = FLY_READ;
	FLY_EVENT_HANDLER(ne, fly_listen_connected);
	ne->flag = FLY_NODELETE;
	fly_sec(&ne->timeout, FLY_REQUEST_TIMEOUT);
	ne->tflag = 0;
	ne->eflag = 0;
	ne->expired = false;
	ne->available = false;
	ne->event_data = __fly_connected(listen_sock, conn_sock, ne,(struct sockaddr *) &addr, addrlen);
	if (ne->event_data == NULL)
		return -1;
	fly_event_socket(ne);

	if (fly_event_register(ne) == -1)
		return -1;

	return 0;
}

__fly_static fly_connect_t *__fly_connected(fly_sock_t fd, fly_sock_t cfd, fly_event_t *e, struct sockaddr *addr, socklen_t addrlen)
{
	fly_connect_t *conn;

	conn = fly_connect_init(fd, cfd, e, addr, addrlen);
	if (conn == NULL)
		return NULL;

	return conn;
}

__fly_static int __fly_worker_open_file(fly_context_t *ctx)
{
	if (!ctx->mount || !ctx->mount->mount_count)
		return -1;

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
					return -1;
				/* pre encode */
				if (fly_over_encoding_threshold(__pf->fs.st_size)){
					struct fly_de *__de;
					int res;

					__de = fly_de_init(__p->pool);
					__de->type = FLY_DE_FROM_PATH;
					__de->fd = __pf->fd;
					__de->offset = 0;
					__de->count = __pf->fs.st_size;
					__de->etype = fly_encoding_from_type(__pf->encode_type);
					if (fly_unlikely_null(__de->decbuf) || \
							fly_unlikely_null(__de->encbuf))
						return -1;

					if ((size_t) __pf->fs.st_size <= FLY_MAX_DE_BUF_SIZE){
						res = __de->etype->encode(__de);
						switch(res){
						case FLY_ENCODE_SUCCESS:
							break;
						case FLY_ENCODE_OVERFLOW:
							FLY_NOT_COME_HERE
						case FLY_ENCODE_ERROR:
							return -1;
						case FLY_ENCODE_SEEK_ERROR:
							return -1;
						case FLY_ENCODE_TYPE_ERROR:
							return -1;
						case FLY_ENCODE_READ_ERROR:
							return -1;
						case FLY_ENCODE_BUFFER_ERROR:
							__de->overflow = true;
							break;
						default:
							return -1;
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
	return 0;
}

__fly_static void __fly_add_rcbs(fly_context_t *ctx, fly_rcbs_t *__r)
{
	fly_bllist_add_tail(&ctx->rcbs, &__r->blelem);
}

/* open worker default content by status code */
extern fly_status_code responses[];
__fly_static int __fly_worker_open_default_content(fly_context_t *ctx)
{
	for (fly_status_code *__res=responses; __res->status_code>0; __res++){
		char env_var[FLY_DEFAULT_CONTENT_PATH_LEN];
		char *defenv;
		int res;
		res = snprintf(env_var, FLY_DEFAULT_CONTENT_PATH_LEN, "FLY_DEFAULT_CONTENT_PATH_%d", __res->status_code);
		defenv = getenv(env_var);
		if (res < 0 || res > FLY_DEFAULT_CONTENT_PATH_LEN)
			continue;
		/* there is a default content path */
		if (defenv || __res->default_path){
			struct fly_response_content_by_stcode *__frc;
			__frc = fly_rcbs_init(ctx);
			if (fly_unlikely_null(__frc))
				return -1;
			__frc->status_code = __res->type;
			__frc->content_path = defenv ? defenv : __res->default_path;
			__frc->mime = fly_mime_type_from_path_name(__frc->content_path);
			__frc->fd = open(__frc->content_path, O_RDONLY);
			if (__frc->fd == -1)
				return -1;

			if (fstat(__frc->fd, &__frc->fs) == -1)
				return -1;

			if (fly_over_encoding_threshold(__frc->fs.st_size)){
				struct fly_de *__de;

				__de = fly_de_init(ctx->pool);
				__de->type = FLY_DE_FROM_PATH;
				__de->fd = __frc->fd;
				__de->offset = 0;
				__de->count = __frc->fs.st_size;
				__de->etype = fly_encoding_from_type(__frc->encode_type);
				if (fly_unlikely_null(__de->decbuf) || \
						fly_unlikely_null(__de->encbuf))
					return -1;
				if (__de->etype->encode(__de) == -1)
					return -1;

				__frc->de = __de;
				__frc->encoded = true;
			}

			__fly_add_rcbs(ctx, __frc);
		}
	}
	return 0;
}

