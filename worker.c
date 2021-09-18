
#define _GNU_SOURCE
#include "worker.h"
#include "fsignal.h"
#include "cache.h"

__fly_static fly_event_t *__fly_listen_socket_event(fly_event_manager_t *manager, fly_context_t *ctx);
__fly_static int __fly_listen_socket_handler(struct fly_event *);
__fly_static fly_connect_t *__fly_connected(fly_sock_t fd, fly_sock_t cfd, fly_event_t *e, struct sockaddr *addr, socklen_t addrlen);
__fly_static int __fly_listen_connected(fly_event_t *);
__fly_static int __fly_worker_signal_event(fly_event_manager_t *manager, __unused fly_context_t *ctx);
__fly_static int __fly_worker_signal_handler(fly_event_t *e);
__fly_static int __fly_add_worker_sigs(fly_context_t *ctx, int num, fly_sighand_t *handler);
__fly_static void FLY_SIGNAL_MODF_HANDLER(__unused fly_context_t *ctx, __unused struct signalfd_siginfo *info);
__fly_static void FLY_SIGNAL_ADDF_HANDLER(__unused fly_context_t *ctx, __unused struct signalfd_siginfo *info);
__fly_static void FLY_SIGNAL_DELF_HANDLER(__unused fly_context_t *ctx, __unused struct signalfd_siginfo *info);
__fly_static void FLY_SIGNAL_UMOU_HANDLER(__unused fly_context_t *ctx, __unused struct signalfd_siginfo *info);
__fly_static int __fly_worker_open_file(fly_context_t *ctx);


#define FLY_WORKER_SIG_COUNT				(sizeof(fly_worker_signals)/sizeof(fly_signal_t))
/*
 *  worker process signal info.
 */
static struct fly_signal *fly_worker_sigptr = NULL;
static fly_signal_t fly_worker_signals[] = {
	{SIGINT,			NULL, NULL},
	{SIGTERM,			NULL, NULL},
};

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

	for (struct fly_mount_parts_file *__f=parts->files; __f; __f=__f->next){
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
	fly_pool_t *pool;

	if (parts->infd == -1)
		return -1;

	pool = parts->mount->ctx->pool;
	__pathd = opendir(path);
	if (__pathd == NULL)
		return -1;

	while((__ent=readdir(__pathd)) != NULL){
		if (strcmp(__ent->d_name, ".") == 0 ||
				strcmp(__ent->d_name, "..") == 0)
			continue;

		if (fly_join_path(__path, parts->mount_path, __ent->d_name) == -1)
			goto error;

		if (fly_isdir(__path) == 1)
			if (__fly_work_add_nftw(parts, __path, mount_point) == -1)
				goto error;

		__pf = fly_pf_from_parts(__path, parts);
		/* already register in parts */
		if (__pf)
			continue;

		/* new file regsiter in parts */
		__pf = fly_pballoc(pool, sizeof(struct fly_mount_parts_file));
		if (fly_unlikely_null(__pf))
			goto error;

		__pf->fd = open(__path, O_RDONLY);
		if (__pf->fd == -1)
			goto error;
		__fly_path_cpy(__pf->filename, __path, mount_point);
		__pf->parts = parts;
		__pf->next = NULL;
		__pf->infd = parts->infd;
		__pf->wd = inotify_add_watch(parts->infd, __path, FLY_INOTIFY_WATCH_FLAG_PF);
		if (__pf->wd == -1)
			goto error;
		if (fly_hash_from_parts_file_path(__path, __pf) == -1)
			goto error;

		fly_parts_file_add(parts, __pf);
	}
	return closedir(__pathd);
error:
	closedir(__pathd);
	return -1;
}

__fly_static int __fly_strcmp_mp(char *filename, char *fullpath, const char *mount_point)
{
	while(*fullpath++ == *mount_point++)
		;

	while(*filename++ == *fullpath++)
		if (*filename == '\0')	return 0;

	return -1;
}

__fly_static int __fly_work_del_nftw(fly_mount_parts_t *parts, __unused char *path, const char *mount_point)
{
	if (parts->file_count == 0)
		return -1;

	char __path[FLY_PATH_MAX];
	struct fly_mount_parts_file *prev;
	for (struct fly_mount_parts_file *__pf=parts->files; __pf; __pf=__pf->next){
		if (fly_join_path(__path, (char *) mount_point, __pf->filename) == -1){
			if (prev == NULL)
				parts->files = __pf->next;
			else
				prev->next = __pf->next;
			/* TODO: release pf */
			if (__pf->fd != -1)
				if (close(__pf->fd) == -1)
					return -1;
			parts->file_count--;
		}
		prev = __pf;
	}

	return 0;
}

__fly_static int __fly_work_unmount(fly_mount_parts_t *parts)
{
	/* close all mount file */
	for (struct fly_mount_parts_file *__pf=parts->files; __pf; __pf=__pf->next){
		if (__pf->fd != -1 && close(__pf->fd) == -1)
			return -1;
		parts->file_count--;
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
	for (fly_mount_parts_t *parts=ctx->mount->parts; parts; parts=parts->next){
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

__fly_static int __fly_wsignal_handle(fly_context_t *ctx, struct signalfd_siginfo *info)
{
	fly_signal_t *__s;
	for (__s=fly_worker_sigptr; __s; __s=__s->next){
		if (__s->number == (fly_signum_t) info->ssi_signo){
			if (__s->handler)
				__s->handler(ctx, info);
			else
				fly_signal_default_handler(info);

			return 0;
		}
	}
	return 0;
}

__fly_static int __fly_worker_signal_handler(__unused fly_event_t *e)
{
	struct signalfd_siginfo info;
	ssize_t res;

	while(1){
		res = read(e->fd, (void *) &info,sizeof(struct signalfd_siginfo));
		if (res == -1){
			if (errno == EAGAIN || errno == EWOULDBLOCK)
				break;
			else
				return -1;
		}
		if (__fly_wsignal_handle(e->manager->ctx, &info) == -1)
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

__fly_static int __fly_worker_signal_event(fly_event_manager_t *manager, __unused fly_context_t *ctx)
{
	sigset_t sset;
	int sigfd;
	fly_event_t *e;

	if (!manager ||  !manager->pool || !ctx)
		return -1;

	if (fly_refresh_signal() == -1)
		return -1;
	if (sigemptyset(&sset) == -1)
		return -1;

	for (int i=0; i<(int) FLY_WORKER_SIG_COUNT; i++){
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
	fly_event_manager_t *manager;
	fly_event_t *event;

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

	manager = fly_event_manager_init(ctx);
	if (manager == NULL)
		FLY_EMERGENCY_ERROR(
			FLY_EMERGENCY_STATUS_READY,
			"initialize event manager error."
		);

	/* initial event */
	/* signal setting */
	if (__fly_worker_signal_event(manager, ctx) == -1)
		FLY_EMERGENCY_ERROR(
			FLY_EMERGENCY_STATUS_READY,
			"initialize worker signal error."
		);

	event = __fly_listen_socket_event(manager, ctx);
	if (event == NULL || fly_event_register(event) == -1)
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

__fly_static fly_event_t *__fly_listen_socket_event(fly_event_manager_t *manager, fly_context_t *ctx)
{
	fly_event_t *e;

	e = fly_event_init(manager);
	if (e == NULL)
		return NULL;

	e->fd = ctx->listen_sock->fd;
	e->read_or_write = FLY_READ;
	FLY_EVENT_HANDLER(e, __fly_listen_socket_handler);
	e->flag = FLY_PERSISTENT;
	e->tflag = FLY_INFINITY;
	e->eflag = 0;
	fly_time_null(e->timeout);
	e->event_data = ctx;
	e->expired = false;
	e->available = false;
	fly_event_socket(e);

	return e;
}

__fly_static int __fly_listen_socket_handler(__unused struct fly_event *event)
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
	FLY_EVENT_HANDLER(ne, __fly_listen_connected);
	ne->flag = FLY_NODELETE;
	ne->tflag = FLY_INFINITY;
	ne->eflag = 0;
	fly_time_null(ne->timeout);
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

/*
 *  ready for receiving request, and publish an received event.
 */
__fly_static int __fly_listen_connected(fly_event_t *e)
{
	fly_connect_t *conn;
	fly_request_t *req;

	conn = (fly_connect_t *) e->event_data;
	/* ready for request */
	req = fly_request_init(conn);
	if (req == NULL)
		return -1;

	e->fd = e->fd;
	e->read_or_write = FLY_READ;
	e->event_data = (void *) req;
	/* event only modify (no add, no delete) */
	e->flag = FLY_MODIFY;
	e->tflag = 0;
	fly_sec(&e->timeout, FLY_REQUEST_TIMEOUT);
	e->eflag = 0;
	FLY_EVENT_HANDLER(e, fly_request_event_handler);
	e->event_state = (void *) EFLY_REQUEST_STATE_INIT;
	e->event_fase = (void *) EFLY_REQUEST_FASE_INIT;
	e->expired = false;
	e->available = false;
	fly_event_socket(e);

	return fly_event_register(e);
}

__fly_static int __fly_worker_open_file(fly_context_t *ctx)
{
	if (!ctx->mount || !ctx->mount->mount_count)
		return -1;

	char rpath[FLY_PATH_MAX];
	for (fly_mount_parts_t *__p=ctx->mount->parts; __p; __p=__p->next){
		if (__p->file_count == 0)
			continue;

		for (struct fly_mount_parts_file *__pf=__p->files; __pf; __pf=__pf->next){
			if (fly_join_path(rpath, __p->mount_path, __pf->filename) == -1)
				continue;

			__pf->fd = open(rpath, O_RDONLY);
		}
	}
	return 0;
}
