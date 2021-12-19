
#define _GNU_SOURCE
#include <unistd.h>
#include <signal.h>
#include "util.h"
#include "master.h"
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
#ifdef HAVE_SIGNALFD
__fly_static int __fly_worker_signal_event(fly_worker_t *worker, fly_event_manager_t *manager, fly_context_t *ctx);
__fly_static int __fly_worker_signal_handler(fly_event_t *e);
#endif
__fly_static void fly_add_worker_sig(fly_context_t *ctx, int num, fly_sighand_t *handler);
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
static void fly_worker_signal_change_mnt_content(__fly_unused fly_context_t *ctx, __fly_unused fly_siginfo_t *info);
#define FLY_PREENCODE_FILE_ENCODE_SUCCESS			0
#define FLY_PREENCODE_FILE_ENCODE_ERROR				-1
#define FLY_PREENCODE_FILE_ENCODE_UNKNOWN_RETURN	-2
static int fly_preencode_pf(fly_context_t *ctx, struct fly_mount_parts_file *__pf);
static int fly_preencode_frc(fly_context_t *ctx, struct fly_response_content_by_stcode *__frc);


#define FLY_WORKER_SIG_COUNT				(sizeof(fly_worker_signals)/sizeof(fly_signal_t))
/*
 *  worker process signal info.
 */
fly_signal_t fly_worker_signals[] = {
	FLY_SIGNAL_SETTING(SIGINT,	FLY_SIG_IGN),
	FLY_SIGNAL_SETTING(SIGPIPE, FLY_SIG_IGN),
	FLY_SIGNAL_SETTING(SIGWINCH, FLY_SIG_IGN),
	FLY_SIGNAL_SETTING(FLY_SIGNAL_CHANGE_MNT_CONTENT, fly_worker_signal_change_mnt_content),
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
#ifdef DEBUG
	assert(__w->context->mount != NULL);
#endif
	__w->context->event_pool = NULL;
	__w->context->pool_manager = __pm;

	/* move to master pool manager to worker pool manager */
	__w->context->pool->manager = __pm;
	__w->context->misc_pool->manager = __pm;
	fly_bllist_add_tail(&__pm->pools, &__w->context->pool->pbelem);
	fly_bllist_add_tail(&__pm->pools, &__w->context->misc_pool->pbelem);
	__w->pid = getpid();
	__w->orig_pid = __w->pid;
	__w->master_pid = getppid();
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
#ifdef DEBUG
	sleep(1);
	printf("WORKER: ==================release resource==================\n");
	printf("WORKER: now event count %d: %d\n", worker->event_manager->monitorable.count, worker->event_manager->unmonitorable.count);
	fflush(stdout);
#endif
	assert(worker != NULL);

	if (worker->event_manager)
		fly_event_manager_release(worker->event_manager);

	/* context is self delete */
	fly_context_release(worker->context);

	fly_pool_manager_release(worker->pool_manager);
	fly_free(worker);
#ifdef DEBUG
	printf("WORKER: ====================================================\n");
	fflush(stdout);
#endif
}

__fly_static void fly_add_worker_sig(fly_context_t *ctx, int num, fly_sighand_t *handler)
{
	fly_worker_t *__w;
	fly_signal_t *__nf;

#ifdef DEBUG
	printf("WORKER SIGNAL SETTING: %d: %s\n", num, strsignal(num));
#endif
	__w = (fly_worker_t *) ctx->data;
	__nf = fly_pballoc(ctx->pool, sizeof(struct fly_signal));
	__nf->number = num;
	__nf->handler = handler;
	fly_bllist_add_tail(&__w->signals, &__nf->blelem);
}

__fly_static void fly_check_mod_file(fly_mount_parts_t *parts)
{
	if (parts->file_count == 0)
		return;

#ifdef DEBUG
	printf("WORKER[%d]: CHECK MODIFIED FILE (%s)\n", getpid(), parts->mount_path);
#endif
	char rpath[FLY_PATH_MAX];

	struct fly_bllist *__b;
	struct fly_mount_parts_file *__f;
	struct stat statbuf;

	fly_for_each_bllist(__b, &parts->files){
		__f = fly_bllist_data(__b, struct fly_mount_parts_file, blelem);
		if (fly_join_path(rpath, FLY_PATH_MAX, parts->mount_path, __f->filename) == -1)
			return;
		if (stat(rpath, &statbuf) == -1)
			FLY_EMERGENCY_ERROR(
				"stat file error in worker. (%s: %d)",
				__FILE__, __LINE__
			);

		if (!fly_pf_modified(&statbuf, __f))
			continue;

#ifdef DEBUG
		printf("WORKER[%d]: DETECT MODIFIED FILE(%s)\n", getpid(), rpath);
		assert(parts->mount != NULL);
		assert(parts->mount->ctx != NULL);
		assert(parts->mount->ctx->log != NULL);
#endif
		FLY_NOTICE_DIRECT_LOG(parts->mount->ctx->log,
			"Worker[%d]: detected a deleted %s(%s).\n",
			getpid(),
			__f->dir ? "directory" : "file", rpath);
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

		if (fly_join_path(__path, FLY_PATH_MAX, path, __ent->d_name) == -1)
			goto error;

		if (fly_isdir(__path) == 1){
			if (__fly_work_add_nftw(parts, __path, mount_point) == -1)
				goto error;
		}
		if (strlen(__path) >= FLY_PATH_MAX)
			continue;
		if (strlen(__ent->d_name) >= FLY_PATHNAME_MAX)
			continue;

		__pf = fly_pf_from_parts_by_fullpath(__path, parts);
		/* already register in parts */
		if (__pf)
			continue;

		/* new file regsiter in parts */
#ifdef DEBUG
		printf("WORKER[%d]: DETECT NEW FILE (%s)\n", getpid(), __path);
#endif
		if (stat(__path, &sb) == -1)
			goto error;

		__pf = fly_pf_init(parts, &sb);
		if (fly_unlikely_null(__pf))
			goto error;

		FLY_NOTICE_DIRECT_LOG(parts->mount->ctx->log,
			"Worker[%d]: detected a new %s(%s).\n",
			getpid(), __pf->dir ? "directory" : "file", __path);
#ifdef HAVE_KQUEUE
		if (fly_isdir(__path) == 1){
			__pf->dir = true;
#ifdef DEBUG
			printf("\tdirectory\n");
		}else{
			printf("\tfile\n");
		}
#else
		}
#endif
#endif
		__pf->fd = open(__path, O_RDONLY);
		if (__pf->fd == -1)
			goto error;

		if (fly_imt_fixdate(__pf->last_modified, FLY_DATE_LENGTH, &__pf->fs.st_mtime) == -1)
			return -1;
		__fly_path_cpy(__pf->filename, __path, mount_point);
		__pf->filename_len = strlen(__pf->filename);
		__pf->parts = parts;
#ifdef HAVE_INOTIFY
		__pf->infd = parts->infd;
#endif
		__pf->mime_type = fly_mime_type_from_path_name(__path);
		if (fly_hash_from_parts_file_path(__path, __pf) == -1)
			goto error;

#ifdef DEBUG
		assert(parts->mount != NULL);
		assert(parts->mount->ctx != NULL);
#endif
		if (!__pf->dir){
			switch (fly_preencode_pf(parts->mount->ctx, __pf)){
			case FLY_PREENCODE_FILE_ENCODE_SUCCESS:
				break;
			case FLY_PREENCODE_FILE_ENCODE_ERROR:
				return -1;
			case FLY_PREENCODE_FILE_ENCODE_UNKNOWN_RETURN:
				return -1;
			default:
				FLY_NOT_COME_HERE
			}
		}

		if (fly_hash_from_parts_file_path(__path, __pf) == -1)
			goto error;
#ifdef DEBUG
		assert(__pf->hash != NULL);
		printf("\tHASH FOR NEW FILE %s\n", __pf->hash->md5);
#endif

		fly_parts_file_add(parts, __pf);
		/* add rbtree of mount */
		fly_rbdata_t data, key, cmpdata;

		fly_rbdata_set_ptr(&data, __pf);
		fly_rbdata_set_ptr(&key, __pf->filename);
		fly_rbdata_set_size(&cmpdata, __pf->filename_len);
		__pf->rbnode = fly_rb_tree_insert(parts->mount->rbtree, &data, &key, &__pf->rbnode, &cmpdata);
	}
	return closedir(__pathd);
error:
	closedir(__pathd);
	return -1;
}

__fly_static int __fly_work_del_nftw(fly_mount_parts_t *parts, const char *mount_point)
{
	int res;
	char __path[FLY_PATH_MAX];
	struct stat stbuf;
	struct fly_mount_parts_file *__pf;
	struct fly_bllist *__b, *__n;

	for (__b=parts->files.next; __b!=&parts->files; __b=__n){
		__n = __b->next;

		__pf = fly_bllist_data(__b, struct fly_mount_parts_file, blelem);

		res = snprintf(__path, FLY_PATH_MAX, "%s/%s", mount_point, __pf->filename);
#ifdef DEBUG
		assert(res < FLY_PATH_MAX);
#endif
		if (res < 0)
			return -1;

		if (stat(__path, &stbuf) == -1 && errno == ENOENT){
#ifdef DEBUG
			printf("WORKER[%d]: DETECT DELETED FILE: (%s)\n", \
					getpid(), __path);
			assert(__pf->fd != -1);
			assert(parts->mount != NULL);
			assert(parts->mount->ctx != NULL);
			assert(parts->mount->ctx->log != NULL);
#endif
			FLY_NOTICE_DIRECT_LOG(parts->mount->ctx->log,
				"Worker[%d]: detected a deleted %s(%s).\n",
				getpid(),
				__pf->dir ? "directory" : "file", __path);
			if (__pf->fd != -1)
				if (close(__pf->fd) == -1)
					return -1;

			fly_parts_file_remove(parts, __pf);
		}
	}

	return 0;
}

__fly_static int __fly_work_unmount(fly_mount_parts_t *parts)
{
	struct fly_mount_parts_file *__pf;
	struct fly_bllist *__b, *__n;

	/* close all mount file */
	for (__b=parts->files.next; __b!=&parts->files; __b=__n){
		__n = __b->next;
		__pf = fly_bllist_data(__b, struct fly_mount_parts_file, blelem);
		if (__pf->fd != -1 && close(__pf->fd) == -1)
			return -1;

		fly_parts_file_remove(parts, __pf);
	}
#ifdef DEBUG
	printf("WORKER[%d]: Unmount mount point. (%s)\n", getpid(), parts->mount_path);
#endif
	return fly_unmount(parts->mount, parts->mount_path);
}

__fly_static void fly_check_add_file(fly_mount_parts_t *parts)
{
#ifdef DEBUG
	printf("WORKER[%d]: CHECK ADD FILE (%s)\n", getpid(), parts->mount_path);
#endif
	if (__fly_work_add_nftw(parts, parts->mount_path, parts->mount_path) == -1){
		FLY_EMERGENCY_ERROR(
			"worker check add file error. (%s: %d)",
			__FILE__, __LINE__
		);
	}
}

__fly_static void fly_check_del_file(fly_mount_parts_t *parts)
{
#ifdef DEBUG
	printf("WORKER[%d]: CHECK DELETE FILE (%s)\n", getpid(), parts->mount_path);
#endif
	if (__fly_work_del_nftw(parts, parts->mount_path) == -1){
		FLY_EMERGENCY_ERROR(
			"worker check delete file error. (%s: %d)",
			__FILE__, __LINE__
		);
	}
}

__fly_noreturn void fly_worker_signal_default_handler(fly_worker_t *worker, fly_context_t *ctx __fly_unused, fly_siginfo_t *si __fly_unused)
{
	fly_notice_direct_log(
		ctx->log,
		"worker process(%d) is received signal(%s) and terminated. goodbye.\n",
		worker->pid,
#ifdef HAVE_SIGNALFD
		strsignal(si->ssi_signo)
#else
		strsignal(si->si_signo)
#endif
	);

	fly_worker_release(worker);
	exit(0);
}

__fly_static int __fly_wsignal_handle(fly_worker_t *worker, fly_context_t *ctx, fly_siginfo_t *info)
{
	struct fly_signal *__s;
	struct fly_bllist *__b;

#ifdef DEBUG
#ifdef HAVE_SIGNALFD
	printf("WORKER: Signal received. %s\n", strsignal(info->ssi_signo));
#else
	printf("WORKER: Signal received. %s\n", strsignal(info->si_signo));
#endif
#endif
	fly_for_each_bllist(__b, &worker->signals){
		__s = fly_bllist_data(__b, struct fly_signal, blelem);
#ifdef HAVE_SIGNALFD
		if (__s->number == (fly_signum_t) info->ssi_signo){
#else
		if (__s->number == (fly_signum_t) info->si_signo){
#endif
			if (__s->handler)
				__s->handler(ctx, info);
			else
				fly_worker_signal_default_handler(worker, ctx, info);

			return 0;
		}
	}

	fly_worker_signal_default_handler(worker, ctx, info);
	return 0;
}

#ifdef HAVE_SIGNALFD
__fly_static int __fly_worker_signal_handler(fly_event_t *e)
{
	fly_siginfo_t info;
	ssize_t res;

#ifdef DEBUG
	printf("WORKER: SIGNAL DEFAULT HANDLER\n");
	assert(e);
	assert(e->manager);
	assert(e->manager->ctx);
	assert(e->manager->ctx->log);
#endif
	FLY_NOTICE_DIRECT_LOG(
		e->manager->ctx->log,
		"Worker[%d]: Received signal.\n",
		getpid()
	);
	while(true){
		res = read(e->fd, (void *) &info,sizeof(fly_siginfo_t));
		if (res == -1){
			if (errno == EAGAIN || errno == EWOULDBLOCK)
				break;
			else
				return -1;
		}
		if (__fly_wsignal_handle((fly_worker_t *) fly_event_data_get(e, __p), e->manager->ctx, &info) == -1)
			return -1;
	}

	return 0;
}
#endif

static int __fly_notice_master_now_pid(fly_worker_t *__w)
{
	union sigval sv;

	memset(&sv, '\0', sizeof(union sigval));
	return fly_send_signal(
		__w->master_pid,
		FLY_NOTICE_WORKER_DAEMON_PID,
		__w->orig_pid
	);
}

#ifdef HAVE_SIGNALFD

static int __fly_worker_signal_end_handler(fly_event_t *__e)
{
	return close(__e->fd);
}

__fly_static int __fly_worker_signal_event(fly_worker_t *worker, fly_event_manager_t *manager, fly_context_t *ctx)
{
	sigset_t sset;
	fly_event_t *e;

	if (!manager ||  !manager->pool || !ctx)
		return FLY_WORKER_SIGNAL_EVENT_INVALID_MANAGER;

	if (fly_refresh_signal() == -1)
		return FLY_WORKER_SIGNAL_EVENT_REFRESH_SIGNAL_ERROR;
	if (sigfillset(&sset) == -1)
		return FLY_WORKER_SIGNAL_EVENT_SIGNAL_FILLSET_ERROR;

	for (int i=0; i<(int) FLY_WORKER_SIG_COUNT; i++)
		fly_add_worker_sig(ctx, fly_worker_signals[i].number, fly_worker_signals[i].handler);

	int sigfd;
	sigfd = fly_signal_register(&sset);
#ifdef DEBUG
	printf("WORKER: signalfd %d\n", sigfd);
#endif
	if (sigfd == -1)
		return FLY_WORKER_SIGNAL_EVENT_SIGNAL_REGISTER_ERROR;

	e = fly_event_init(manager);
	if (fly_unlikely_null(e))
		return FLY_WORKER_SIGNAL_EVENT_INIT_ERROR;

	e->fd = sigfd;
	e->read_or_write = FLY_READ;
	e->tflag = FLY_INFINITY;
	e->eflag = 0;
	e->flag = FLY_PERSISTENT;
	e->expired = false;
	e->available = false;
	FLY_EVENT_HANDLER(e, __fly_worker_signal_handler);
	FLY_EVENT_END_HANDLER(e, __fly_worker_signal_end_handler, NULL);
	e->if_fail_term = true;
	fly_event_data_set(e, __p, worker);

	fly_time_null(e->timeout);
	fly_event_signal(e);
	return fly_event_register(e);
}
#else
static fly_worker_t *__wptr;

static void __fly_worker_sigaction(int signum __fly_unused, fly_siginfo_t *info, void *ucontext __fly_unused)
{
#ifdef DEBUG
	assert(__wptr != NULL);
	assert(__wptr->context != NULL);
	printf("WORKER[%d]: Received signal. %s\n", getpid(), strsignal(info->si_signo));
#endif
	FLY_NOTICE_DIRECT_LOG(
		__wptr->context->log,
		"Worker[%d]: Received signal. %s\n",
		getpid(), strsignal(info->si_signo)
	);
	__fly_wsignal_handle(__wptr, __wptr->context, info);
}

__fly_static int __fly_worker_signal(fly_worker_t *worker, fly_event_manager_t *manager __fly_unused, fly_context_t *ctx)
{
#define FLY_KQUEUE_WORKER_SIGNALSET(signum)						\
		do{													\
			struct sigaction __sa;							\
			memset(&__sa, '\0', sizeof(struct sigaction));	\
			if (sigfillset(&__sa.sa_mask) == -1)			\
				return -1;									\
			__sa.sa_sigaction = __fly_worker_sigaction;		\
			__sa.sa_flags = SA_SIGINFO|SA_NODEFER;			\
			if (sigaction((signum), &__sa, NULL) == -1)		\
				return -1;									\
		} while(0)

	if (fly_refresh_signal() == -1)
		return -1;

	if (sigaddset(&manager->signal_mask, FLY_SIGNAL_CHANGE_MNT_CONTENT) == -1)
		return -1;
#ifdef DEBUG
	printf("WORKER SIGNAL SETTING COUNT: \"%ld\"\n", FLY_WORKER_SIG_COUNT);
#endif
	for (int i=0; i<(int) FLY_WORKER_SIG_COUNT; i++){
		fly_add_worker_sig(ctx, fly_worker_signals[i].number, fly_worker_signals[i].handler);
	}
//	fly_worker_rtsig_added(ctx);

	__wptr = worker;
	FLY_KQUEUE_WORKER_SIGNALSET(SIGABRT);
	FLY_KQUEUE_WORKER_SIGNALSET(SIGALRM);
	FLY_KQUEUE_WORKER_SIGNALSET(SIGBUS);
	FLY_KQUEUE_WORKER_SIGNALSET(SIGCHLD);
	FLY_KQUEUE_WORKER_SIGNALSET(SIGCONT);
	FLY_KQUEUE_WORKER_SIGNALSET(SIGFPE);
	FLY_KQUEUE_WORKER_SIGNALSET(SIGHUP);
	FLY_KQUEUE_WORKER_SIGNALSET(SIGILL);
	FLY_KQUEUE_WORKER_SIGNALSET(SIGINFO);
	FLY_KQUEUE_WORKER_SIGNALSET(SIGINT);
	FLY_KQUEUE_WORKER_SIGNALSET(SIGPIPE);
#ifdef SIGPOLL
	FLY_KQUEUE_WORKER_SIGNALSET(SIGPOLL);
#endif
	FLY_KQUEUE_WORKER_SIGNALSET(SIGPROF);
	FLY_KQUEUE_WORKER_SIGNALSET(SIGQUIT);
	FLY_KQUEUE_WORKER_SIGNALSET(SIGSEGV);
	FLY_KQUEUE_WORKER_SIGNALSET(SIGTSTP);
	FLY_KQUEUE_WORKER_SIGNALSET(SIGSYS);
	FLY_KQUEUE_WORKER_SIGNALSET(SIGTERM);
	FLY_KQUEUE_WORKER_SIGNALSET(SIGTRAP);
	FLY_KQUEUE_WORKER_SIGNALSET(SIGTTIN);
	FLY_KQUEUE_WORKER_SIGNALSET(SIGTTOU);
	FLY_KQUEUE_WORKER_SIGNALSET(SIGURG);
	FLY_KQUEUE_WORKER_SIGNALSET(SIGUSR1);
	FLY_KQUEUE_WORKER_SIGNALSET(SIGUSR2);
	FLY_KQUEUE_WORKER_SIGNALSET(SIGVTALRM);
	FLY_KQUEUE_WORKER_SIGNALSET(SIGXCPU);
	FLY_KQUEUE_WORKER_SIGNALSET(SIGXFSZ);
	FLY_KQUEUE_WORKER_SIGNALSET(SIGWINCH);

#if defined SIGRTMIN && defined SIGRTMAX
	for (int i=SIGRTMIN; i<SIGRTMAX; i++)
		FLY_KQUEUE_WORKER_SIGNALSET(i);
#endif

	sigset_t sset;
	if (sigfillset(&sset) == -1)
		return -1;
	if (sigprocmask(SIG_UNBLOCK, &sset, NULL) == -1)
		return -1;

	return 0;
}
#endif

/*
 * this function is called after fork from master process.
 * daemon process.
 * @params:
 *		ctx:  passed from master process. include fly context info.
 *		data: custom data.
 */
__fly_direct_log __fly_noreturn void fly_worker_process(fly_context_t *ctx, __fly_unused void *data)
{
#ifdef DEBUG_EMERGE
	FLY_EMERGENCY_ERROR(
		"Worker emergency test."
	);
#endif
	fly_worker_t *worker;
	fly_event_manager_t *manager;

	worker = (fly_worker_t *) data;
	if (!ctx)
		FLY_EMERGENCY_ERROR(
			"worker context is invalid(null context)."
		);
	ctx->data = (void *) worker;
	fly_errsys_init(ctx);

	worker->pid = getpid();
	/* prevent from keyboard interrupts */
	int __ifd;
	if ((__ifd = open(FLY_DEVNULL, O_RDWR)) == -1)
		FLY_EMERGENCY_ERROR(
			"worker open %s error. (%s: %d)",
			FLY_ROOT_DIR,
			__FILE__, __LINE__
		);
	if (dup2(__ifd, STDIN_FILENO) == -1)
		FLY_EMERGENCY_ERROR(
			"worker dup error. (%s: %d)",
			__FILE__, __LINE__
		);

	if (__fly_notice_master_now_pid(worker) == -1)
		FLY_EMERGENCY_ERROR(
			"worker notice daemon pid error. (%s: %d)",
			__FILE__, __LINE__
		);

	switch (__fly_worker_open_file(ctx)){
	case FLY_WORKER_OPEN_FILE_SUCCESS:
		break;
	case FLY_WORKER_OPEN_FILE_CTX_ERROR:
		FLY_EMERGENCY_ERROR(
			"worker opening file error. worker context is invalid. (%s: %d)",
			__FILE__, __LINE__
		);
		break;
	case FLY_WORKER_OPEN_FILE_SETTING_DATE_ERROR:
		FLY_EMERGENCY_ERROR(
			"worker opening file error. occurred error when solving time. (%s: %d)",
			__FILE__, __LINE__
		);
		break;
	case FLY_WORKER_OPEN_FILE_ENCODE_ERROR:
		FLY_EMERGENCY_ERROR(
			"worker opening file error. occurred error when encoding opening file. (%s: %d)",
			__FILE__, __LINE__
		);
		break;
	case FLY_WORKER_OPEN_FILE_ENCODE_UNKNOWN_RETURN:
		FLY_EMERGENCY_ERROR(
			"worker opening file error. unknown return value in encoding opening file. (%s: %d)",
			__FILE__, __LINE__
		);
		break;
	default:
		FLY_EMERGENCY_ERROR(
			"worker opening file error. unknown return value. (%s: %d)",
			__FILE__, __LINE__
		);
	}

#ifdef DEBUG
	printf("worker pid %d\n", getpid());
#endif
	switch(__fly_worker_open_default_content(ctx)){
	case FLY_WORKER_OPEN_DEFAULT_CONTENT_NOCONTENT:
		break;
	case FLY_WORKER_OPEN_DEFAULT_CONTENT_NODIR:
		break;
	case FLY_WORKER_OPEN_DEFAULT_CONTENT_SUCCESS:
		break;
	case FLY_WORKER_OPEN_DEFAULT_CONTENT_SOLVPATH_ERROR:
		FLY_EMERGENCY_ERROR(
			"worker opening default content error. solving path error in opening worker default content. (%s: %d)",
			__FILE__, __LINE__
		);
		break;
	case FLY_WORKER_OPEN_DEFAULT_CONTENT_FRC_INIT_ERROR:
		FLY_EMERGENCY_ERROR(
			"worker opening default content error. frc init error in opening worker default content. (%s: %d)",
			__FILE__, __LINE__
		);
		break;
	case FLY_WORKER_OPEN_DEFAULT_CONTENT_INVALID_FILE:
		FLY_EMERGENCY_ERROR(
			"worker opening default content error. found invalid file in opening worker default content. (%s: %d)",
			__FILE__, __LINE__
		);
		break;
	case FLY_WORKER_OPEN_DEFAULT_CONTENT_ENCODE_ERROR:
		FLY_EMERGENCY_ERROR(
			"worker opening default content error. occurred error when encoding default content. (%s: %d)",
			__FILE__, __LINE__
		);
		break;
	case FLY_WORKER_OPEN_DEFAULT_CONTENT_ENCODE_UNKNOWN_RETURN:
		FLY_EMERGENCY_ERROR(
			"worker opening default content error. unknown return value in encoding default content. (%s: %d)",
			__FILE__, __LINE__
		);
		break;
	default:
		FLY_EMERGENCY_ERROR(
			"worker open default content error.	unknown return value in opening default content. (%s: %d)",
			__FILE__, __LINE__
		);
	}

	manager = fly_event_manager_init(ctx);
	if (manager == NULL)
		FLY_EXIT_ERROR(
			"worker event manager init error. %s (%s: %d)",
			strerror(errno),
			__FILE__, __LINE__
		);

	worker->event_manager = manager;
	/* initial event */
	/* signal setting */
#ifdef HAVE_SIGNALFD
	if (__fly_worker_signal_event(worker, manager, ctx) == -1)
		FLY_EMERGENCY_ERROR(
			"initialize worker signal error. (%s: %d)",
			__FILE__, __LINE__
		);
#else
	if (__fly_worker_signal(worker, manager, ctx) == -1)
		FLY_EMERGENCY_ERROR(
			"initialize worker signal error. (%s: %d)",
			__FILE__, __LINE__
		);
#endif

	/* make socket for each socket info */
	if (fly_wainting_for_connection_event(manager, ctx->listen_sock) == -1)
		FLY_EMERGENCY_ERROR(
			"fail to register listen socket event. (%s: %d)",
			__FILE__, __LINE__
		);

#ifdef DEBUG
	assert(ctx->log != NULL);
	assert(ctx->log->access != NULL);
	assert(ctx->log->error != NULL);
	assert(ctx->log->notice != NULL);
	assert(ctx->log->debug != NULL);
	printf("WORKER: ACCESS LOG PATH: path %s, fd %d, %s, flag %d\n", ctx->log->access->log_path, ctx->log->access->file, ctx->log->access->tty ? "tty" : "notty", ctx->log->access->flag);
	printf("WORKER: ERROR LOG PATH: path %s, fd %d, %s, flag %d\n", ctx->log->error->log_path, ctx->log->error->file, ctx->log->error->tty ? "tty" : "notty", ctx->log->error->flag);
	printf("WORKER: NOTICE LOG PATH: path %s, fd %d, %s, flag %d\n", ctx->log->notice->log_path, ctx->log->notice->file, ctx->log->notice->tty ? "tty" : "notty", ctx->log->notice->flag);
	printf("WORKER: DEBUG LOG PATH: path %s, fd %d, %s, flag %d\n", ctx->log->debug->log_path, ctx->log->debug->file, ctx->log->debug->tty ? "tty" : "notty", ctx->log->debug->flag);
	FLY_NOTICE_DIRECT_LOG(
		ctx->log,
		"notice log test on worker(%d).\n",
		getpid()
	);
#endif
	/* log event start here */
#ifdef DEBUG
	printf("WORKER PROCESS EVENT START!\n");
#endif
	switch(fly_event_handler(manager)){
	case FLY_EVENT_HANDLER_INVALID_MANAGER:
		FLY_EMERGENCY_ERROR(
			"event handle error. event manager is invalid. (%s: %d)",
			__FILE__, __LINE__
		);
		break;
	case FLY_EVENT_HANDLER_EPOLL_ERROR:
		FLY_EMERGENCY_ERROR(
			"event handle error. epoll was broken. (%s: %d)",
			__FILE__, __LINE__
		);
		break;
	case FLY_EVENT_HANDLER_EXPIRED_EVENT_ERROR:
		FLY_EMERGENCY_ERROR(
			"event handle error. occurred error in expired event handler. (%s: %d)",
			__FILE__, __LINE__
		);
		break;
	default:
		FLY_NOT_COME_HERE
	}

	/* will not come here. */
	FLY_NOT_COME_HERE
}

static int __fly_waiting_for_connection_end_handler(fly_event_t *__e)
{
	return close(__e->fd);
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
	fly_event_data_set(e, __p, sockinfo);
	e->expired = false;
	e->available = false;
	e->if_fail_term = true;
	e->end_handler = __fly_waiting_for_connection_end_handler;
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
			if (fly_join_path(rpath, FLY_PATH_MAX, __p->mount_path, __pf->filename) == -1)
				continue;
#ifdef DEBUG
			assert(strlen(rpath) <= FLY_PATH_MAX);
#endif
			__pf->fd = open(rpath, O_RDONLY);
			/* if index, setting */
			const char *index_path = fly_index_path();
			if (strncmp(__pf->filename, index_path, strlen(index_path)) == 0)
				fly_mount_index_parts_file(__pf);

			if (fly_imt_fixdate(__pf->last_modified, FLY_DATE_LENGTH, &__pf->fs.st_mtime) == -1)
				return FLY_WORKER_OPEN_FILE_SETTING_DATE_ERROR;

			switch(fly_preencode_pf(ctx, __pf)){
			case FLY_PREENCODE_FILE_ENCODE_SUCCESS:
				break;
			case FLY_PREENCODE_FILE_ENCODE_ERROR:
				return FLY_WORKER_OPEN_FILE_ENCODE_ERROR;
			case FLY_PREENCODE_FILE_ENCODE_UNKNOWN_RETURN:
				return FLY_WORKER_OPEN_FILE_ENCODE_UNKNOWN_RETURN;
			default:
				FLY_NOT_COME_HERE
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

		switch(fly_preencode_frc(ctx, __frc)){
		case FLY_PREENCODE_FILE_ENCODE_SUCCESS:
			break;
		case FLY_PREENCODE_FILE_ENCODE_ERROR:
			return FLY_WORKER_OPEN_DEFAULT_CONTENT_ENCODE_ERROR;
		case FLY_PREENCODE_FILE_ENCODE_UNKNOWN_RETURN:
			return FLY_WORKER_OPEN_DEFAULT_CONTENT_ENCODE_UNKNOWN_RETURN;
		default:
			FLY_NOT_COME_HERE
		}

		__fly_add_rcbs(ctx, __frc);
	}
	return FLY_WORKER_OPEN_DEFAULT_CONTENT_SUCCESS;
}

const char *fly_default_content_path(void)
{
	return (const char *) fly_config_value_str(FLY_DEFAULT_CONTENT_PATH);
}

static void fly_worker_signal_change_mnt_content(fly_context_t *ctx, __fly_unused fly_siginfo_t *info)
{
#ifdef DEBUG
	assert(ctx != NULL);
	assert(ctx->mount != NULL);
	assert(ctx->mount->mount_count > 0);
	printf("WORKER: MOUNT FILE COUNT: %ld\n", ctx->mount->file_count);
	printf("WORKER: MOUNT RBNODE COUNT: %ld\n", ctx->mount->rbtree->node_count);
#endif
	struct fly_mount_parts *__p;
	struct fly_bllist *__b;

	fly_for_each_bllist(__b, &ctx->mount->parts){
		__p = (struct fly_mount_parts *) fly_bllist_data(__b, fly_mount_parts_t, mbelem);
		struct stat __sb;

		if (stat(__p->mount_path, &__sb) == -1){
			if (errno == ENOENT){
#ifdef DEBUG
				int __tmp, __tmpc;
				size_t __tmpm;
				__tmp = __p->file_count;
				__tmpm = __p->mount->file_count;
				__tmpc = __p->mount->mount_count;

				printf("WORKER[%d]: DETECT UNMOUNT MOUNT POINT(%s)\n", getpid(), __p->mount_path);
#endif
				FLY_NOTICE_DIRECT_LOG(ctx->log,
					"Worker[%d]: detected that mount point is unmounted(%s).\n",
					getpid(), __p->mount_path);
				/* unmount mount point */
				__fly_work_unmount(__p);
#ifdef DEBUG
				printf("\tUnmount file count %d\n", __tmp);
				printf("\tMount point count %d --> %d\n", __tmpc, __p->mount->mount_count);
				printf("\tTotal file count %ld --> %ld\n", __tmpm, __p->mount->file_count);
				assert(__tmpm == __p->mount->file_count + __tmp);
#endif
				continue;
			}

			/* emergency error */
			FLY_EMERGENCY_ERROR(
				"unknown stat error %s in worker's signal handler of change of mount path content.(%s: %d)", __p->mount_path,  __FILE__, __LINE__
			);
		}

		fly_check_add_file(__p);
		fly_check_del_file(__p);
		fly_check_mod_file(__p);
	}
#ifdef DEBUG
	printf("WORKER MOUNT CONTENT DEBUG\n");
	__fly_debug_mnt_content(ctx);
	FLY_NOTICE_DIRECT_LOG(
		ctx->log,
		"Worker[%d]: End of inotify.\n",
		getpid()
	);
#endif
	return;
}


static int fly_preencode_pf(fly_context_t *ctx, struct fly_mount_parts_file *__pf)
{
	if (!fly_over_encoding_threshold(ctx, (size_t) __pf->fs.st_size) || \
			!S_ISREG(__pf->fs.st_mode))
		return 0;

	struct fly_de *__de;
	int res;

	__de = fly_de_init(__pf->parts->pool);
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
			return FLY_PREENCODE_FILE_ENCODE_ERROR;
		case FLY_ENCODE_SEEK_ERROR:
			return FLY_PREENCODE_FILE_ENCODE_ERROR;
		case FLY_ENCODE_TYPE_ERROR:
			return FLY_PREENCODE_FILE_ENCODE_ERROR;
		case FLY_ENCODE_READ_ERROR:
			return FLY_PREENCODE_FILE_ENCODE_ERROR;
		case FLY_ENCODE_BUFFER_ERROR:
			__de->overflow = true;
			break;
		default:
			return FLY_PREENCODE_FILE_ENCODE_UNKNOWN_RETURN;
		}
	}else{
		__de->overflow = true;
		__pf->overflow = true;
	}

	__pf->de = __de;
	__pf->encoded = true;
	return FLY_PREENCODE_FILE_ENCODE_SUCCESS;
}

static int fly_preencode_frc(fly_context_t *ctx, struct fly_response_content_by_stcode *__frc)
{
	if (!fly_over_encoding_threshold(ctx, (size_t) __frc->fs.st_size) || \
			!S_ISREG(__frc->fs.st_mode))
		return FLY_PREENCODE_FILE_ENCODE_SUCCESS;

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
			return FLY_PREENCODE_FILE_ENCODE_ERROR;
		case FLY_ENCODE_SEEK_ERROR:
			return FLY_PREENCODE_FILE_ENCODE_ERROR;
		case FLY_ENCODE_TYPE_ERROR:
			return FLY_PREENCODE_FILE_ENCODE_ERROR;
		case FLY_ENCODE_READ_ERROR:
			return FLY_PREENCODE_FILE_ENCODE_ERROR;
		case FLY_ENCODE_BUFFER_ERROR:
			__de->overflow = true;
			break;
		default:
			return FLY_PREENCODE_FILE_ENCODE_UNKNOWN_RETURN;
		}
	}else{
		__de->overflow = true;
	}

	__frc->de = __de;
	__frc->encoded = true;
	return FLY_PREENCODE_FILE_ENCODE_SUCCESS;
}
