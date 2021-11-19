#include "master.h"
#include "alloc.h"
#include "util.h"
#include "cache.h"
#include "conf.h"
#include <setjmp.h>
#include "err.h"

int __fly_master_fork(fly_master_t *master, fly_proc_type type, void (*proc)(fly_context_t *, void *), fly_context_t *ctx);
__fly_static int __fly_master_signal_event(fly_master_t *master, fly_event_manager_t *manager, __unused fly_context_t *ctx);
__fly_static int __fly_msignal_handle(fly_master_t *master, fly_context_t *ctx, struct signalfd_siginfo *info);
__fly_static int __fly_master_signal_handler(fly_event_t *);
__fly_static void __fly_workers_rebalance(fly_master_t *master);
__fly_static void __fly_sigchld(fly_context_t *ctx, struct signalfd_siginfo *info);
__fly_static int __fly_master_inotify_event(fly_master_t *master, fly_event_manager_t *manager);
__fly_static int __fly_master_inotify_handler(fly_event_t *);
__fly_static void fly_add_worker(fly_master_t *m, fly_worker_t *w);
__fly_static void fly_remove_worker(fly_master_t *m, pid_t cpid);
__noreturn static void fly_master_signal_default_handler(fly_master_t *master, fly_context_t *ctx __unused, struct signalfd_siginfo *si __unused);
static int __fly_reload(fly_master_t *__m, struct inotify_event *__ie);
static int __fly_master_reload_filepath(fly_master_t *master, fly_event_manager_t *manager);
static int __fly_master_reload_filepath_handler(fly_event_t *e);
static struct fly_watch_path *__fly_search_wp(fly_master_t *__m, int wd);
static int fly_master_default_fail_close(fly_event_t *e, int fd);
#define FLY_MASTER_SIG_COUNT				(sizeof(fly_master_signals)/sizeof(fly_signal_t))
#if defined HAVE_SIGLONGJMP && defined HAVE_SIGSETJMP
static sigjmp_buf env;
#else
static jmp_buf env;
#endif

fly_signal_t fly_master_signals[] = {
	FLY_SIGNAL_SETTING(SIGCHLD,	__fly_sigchld),
	FLY_SIGNAL_SETTING(SIGINT,	NULL),
	FLY_SIGNAL_SETTING(SIGTERM, NULL),
};

int fly_master_daemon(fly_context_t *ctx)
{
	struct rlimit fd_limit;
	int nullfd;

	ctx->daemon = true;
	switch(fork()){
	case -1:
		FLY_STDERR_ERROR(
			"failure to fork process from baemon source process."
		);
	case 0:
		break;
	default:
		exit(0);
	}

	/* child process only */
	if (setsid() == -1)
		FLY_EMERGENCY_ERROR(
			"Process(%d) must not be process group leader.",
			getpid()
		);

	/* for can't access tty */
	switch(fork()){
	case -1:
		FLY_EMERGENCY_ERROR(
			"failure to fork process from baemon source process."
		);
	case 0:
		break;
	default:
		exit(0);
	}
	/* grandchild process only */
	umask(0);
	if (chdir(FLY_ROOT_DIR) == -1)
		FLY_EMERGENCY_ERROR(
			"can't change directory. path(%s).",
			FLY_ROOT_DIR
		);

	if (getrlimit(RLIMIT_NOFILE, &fd_limit) == -1)
		FLY_EMERGENCY_ERROR(
			"can't get resource of RLIMIT_NOFILE."
		);

	for (int i=0; i<(int) fd_limit.rlim_cur; i++){
		if (is_fly_log_fd(i, ctx))
			continue;
		if (is_fly_listen_socket(i, ctx))
			continue;

		if (close(i) == -1 && errno != EBADF)
			FLY_EMERGENCY_ERROR(
				"can't close file (fd: %d)", i
			);
	}

	nullfd = open(__FLY_DEVNULL, O_RDWR);
	if (nullfd == -1 || nullfd != STDIN_FILENO)
		FLY_EMERGENCY_ERROR(
			"can't open file (fd: %d)", nullfd
		);

	if (dup2(nullfd, STDOUT_FILENO) == -1)
		FLY_EMERGENCY_ERROR(
			"can't duplicate file %d->%d", nullfd, STDOUT_FILENO
		);
	if (dup2(nullfd, STDERR_FILENO) == -1)
		FLY_EMERGENCY_ERROR(
			"can't duplicate file %d->%d", nullfd, STDERR_FILENO
		);
	return 0;
}


static int fly_worker_max_limit(void)
{
	return fly_config_value_int(FLY_WORKER_MAX);
}
/*
 *  adjust workers number.
 *
 */
__fly_static void __fly_workers_rebalance(fly_master_t *master)
{
	if (master->now_workers <= fly_worker_max_limit()){
		fly_master_worker_spawn(
			master,
			master->worker_process
		);
	}
}

__fly_static void __fly_sigchld(fly_context_t *ctx, struct signalfd_siginfo *info)
{
	fly_master_t *master;

	master = (fly_master_t *) ctx->data;
	switch(info->ssi_code){
	case CLD_CONTINUED:
		printf("continued\n");
		goto decrement;
	case CLD_DUMPED:
		FLY_NOTICE_DIRECT_LOG(
			ctx->log,
			"master process(%d) detected the dumped of worker process(%d).",
			getpid(),
			info->ssi_pid,
			info->ssi_status
		);
		goto decrement;
	case CLD_EXITED:
		printf("exited\n");
		/* end status of worker(error level) */
		switch(info->ssi_status){
		case FLY_WORKER_SUCCESS_EXIT:
			goto decrement;
		case FLY_ERR_EMERG:
			FLY_NOTICE_DIRECT_LOG(
				ctx->log,
				"master process(%d) detected the emergency termination of worker process(%d).",
				getpid(),
				info->ssi_pid,
				info->ssi_status
			);
			/* terminate fly processes. */
			goto fly_terminate;
		case FLY_ERR_CRIT:
			FLY_NOTICE_DIRECT_LOG(
				ctx->log,
				"master process(%d) detected the critical termination of worker process(%d).",
				getpid(),
				info->ssi_pid,
				info->ssi_status
			);
			/* terminate fly processes. */
			goto fly_terminate;
		case FLY_ERR_ERR:
			FLY_NOTICE_DIRECT_LOG(
				ctx->log,
				"master process(%d) detected the error termination of worker process(%d).",
				getpid(),
				info->ssi_pid,
				info->ssi_status
			);

			goto decrement;
		default:
#ifndef DEBUG
			assert(0);
#endif
			FLY_EMERGENCY_ERROR(
				"unknown worker exit status. (%d)",
				info->ssi_status
			);
			goto decrement;
		}
		FLY_NOT_COME_HERE
	case CLD_KILLED:
		printf("killed\n");
		goto decrement;
	case CLD_STOPPED:
		printf("stopped\n");
		goto decrement;
	case CLD_TRAPPED:
		printf("trapped\n");
		goto decrement;
	default:
		FLY_EMERGENCY_ERROR(
			"unknown signal code. (%d)",
			info->ssi_code
		);
	}

decrement:
	fly_remove_worker((fly_master_t *) ctx->data, (pid_t) info->ssi_pid);
	__fly_workers_rebalance(master);
	return;

fly_terminate:
	/* fly all processes(workers/master) terminate */
	fly_master_signal_default_handler(master, ctx, info);
	return;
}

__noreturn static void fly_master_signal_default_handler(fly_master_t *master, fly_context_t *ctx __unused, struct signalfd_siginfo *si __unused)
{
	struct fly_bllist *__b;
	fly_worker_t *__w;

retry:
	fly_for_each_bllist(__b, &master->workers){
		__w = fly_bllist_data(__b, fly_worker_t, blelem);
		fly_send_signal(__w->pid, si->ssi_signo, 0);

		fly_remove_worker(master, __w->pid);
		goto retry;
	}

	fly_master_release(master);

	/* jump to master process */
#if defined HAVE_SIGLONGJMP && defined HAVE_SIGSETJMP
	siglongjmp(env, FLY_MASTER_SIGNAL_END);
#else
	longjmp(env, FLY_MASTER_SIGNAL_END);
#endif
}

__fly_static int __fly_msignal_handle(fly_master_t *master, fly_context_t *ctx, struct signalfd_siginfo *info)
{

	for (int i=0; i<(int) FLY_MASTER_SIG_COUNT; i++){
		fly_signal_t *__s = &fly_master_signals[i];
		if (__s->number == (fly_signum_t) info->ssi_signo){
			if (__s->handler)
				__s->handler(ctx, info);
			else
				fly_master_signal_default_handler(master, ctx, info);
		}
	}
	return 0;
}

__fly_static int __fly_master_signal_handler(fly_event_t *e)
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
		if (__fly_msignal_handle((fly_master_t *) e->event_data, e->manager->ctx, &info) == -1)
			return -1;
	}

	return 0;
}

__fly_static int __fly_master_signal_event(fly_master_t *master, fly_event_manager_t *manager, __unused fly_context_t *ctx)
{
	sigset_t master_set;
	fly_event_t *e;
	int sigfd;

	if (fly_refresh_signal() == -1)
		return -1;
	if (sigfillset(&master_set) == -1)
		return -1;

	sigfd = fly_signal_register(&master_set);
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
	e->event_data = (void *) master;
	e->if_fail_term = true;
	e->fail_close = fly_master_default_fail_close;
	FLY_EVENT_HANDLER(e, __fly_master_signal_handler);

	fly_time_null(e->timeout);
	fly_event_signal(e);
	return fly_event_register(e);
}

static int fly_workers_count(void)
{
	return fly_config_value_int(FLY_WORKER);
}

void fly_master_release(fly_master_t *master)
{
	assert(master != NULL);

	if (master->event_manager)
		fly_event_manager_release(master->event_manager);

	fly_context_release(master->context);
	fly_pool_manager_release(master->pool_manager);

	if (master->reload_pathcount > 0){
		struct fly_bllist *__b;
		struct fly_watch_path *__p;
		fly_for_each_bllist(__b, &master->reload_filepath){
			__p = (struct fly_watch_path *) fly_bllist_data(__b, struct fly_watch_path, blelem);
			fly_free(__p);
			master->reload_pathcount--;
		}
	}
	fly_free(master);
}

fly_context_t *fly_master_release_except_context(fly_master_t *master)
{
	assert(master != NULL);

	fly_context_t *ctx = master->context;
	ctx->pool_manager = NULL;

	if (master->event_manager)
		fly_event_manager_release(master->event_manager);
	fly_pool_manager_release(master->pool_manager);
	fly_free(master);

	return ctx;
}

fly_master_t *fly_master_init(void)
{
	struct fly_pool_manager *__pm;
	fly_master_t *__m;
	fly_context_t *__ctx;

	__pm = fly_pool_manager_init();
	if (fly_unlikely_null(__pm))
		return NULL;

	__m = fly_malloc(sizeof(fly_master_t));
	if (fly_unlikely_null(__m))
		return NULL;

	__ctx = fly_context_init(__pm);
	if (fly_unlikely_null(__ctx))
		return NULL;

	__m->pid = getpid();
	__m->req_workers = fly_workers_count();
	__m->now_workers = 0;
	__m->worker_process = NULL;
	__m->pool_manager = __pm;
	__m->event_manager = NULL;
	fly_bllist_init(&__m->workers);
	__m->context = __ctx;
	__m->context->data = __m;
	fly_bllist_init(&__m->reload_filepath);
	__m->reload_pathcount = 0;
	__m->detect_reload = false;

	return __m;
}

void fly_kill_workers(fly_context_t *ctx)
{
	fly_master_t *master;
	fly_worker_t *__w;
	struct fly_bllist *__b;

	master = (fly_master_t *) ctx->data;
retry:
	fly_for_each_bllist(__b, &master->workers){
		__w = fly_bllist_data(__b, fly_worker_t, blelem);
		fly_send_signal(__w->pid, SIGINT, 0);

		fly_remove_worker(master, __w->pid);
		goto retry;
	}
	master->now_workers = 0;
}

__direct_log int fly_master_process(fly_master_t *master)
{
	fly_event_manager_t *manager;
	int res;

	manager = fly_event_manager_init(master->context);
	if (manager == NULL)
		FLY_EMERGENCY_ERROR(
			"master initialize event manager error. %s",
			strerror(errno)
		);
	fly_jbhandle_setting(manager, fly_kill_workers);

	master->event_manager = manager;
	/* initial event setting */
	if (__fly_master_signal_event(master, manager, master->context) == -1)
		FLY_EMERGENCY_ERROR(
			"initialize worker signal error. %s",
			strerror(errno)
		);
	if (__fly_master_inotify_event(master, manager) == -1)
		FLY_EMERGENCY_ERROR(
			"initialize worker inotify error. %s",
			strerror(errno)
		);
	if (__fly_master_reload_filepath(master, manager) == -1)
		FLY_EMERGENCY_ERROR(
			"setting master reload file error. %s",
			strerror(errno)
		);
#if defined HAVE_SIGLONGJMP && defined HAVE_SIGSETJMP
	res = sigsetjmp(env, 1);
#else
	res = setjmp(env);
#endif
	if (res == FLY_MASTER_CONTINUE){
		/* event handler start here */
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
	}else if (res == FLY_MASTER_SIGNAL_END){
		/* signal or reload file return */
		return res;
	}else if (res == FLY_MASTER_RELOAD){
		return res;
	}

	/* will not come here. */
	FLY_NOT_COME_HERE
}

int fly_create_pidfile_noexit(void)
{
	pid_t pid;
	int pidfd, res;
	char pidbuf[FLY_PID_MAXSTRLEN];

	pidfd = open(FLY_PID_FILE, O_WRONLY|O_CREAT|O_EXCL, S_IRUSR|S_IWUSR);
	if (pidfd == -1)
		return -1;

	memset(pidbuf, 0, FLY_PID_MAXSTRLEN);
	pid = fly_master_pid();
	if (pid == -1)
		return -1;
	res = snprintf(pidbuf, FLY_PID_MAXSTRLEN, "%ld", (long) pid);
	if (res < 0 || res >= FLY_PID_MAXSTRLEN)
		return -1;

	if (write(pidfd, pidbuf, strlen(pidbuf)) == -1)
		return -1;

	if (close(pidfd) == -1)
		return -1;
	return 0;
}

int fly_create_pidfile(void)
{
	pid_t pid;
	int pidfd, res;
	char pidbuf[FLY_PID_MAXSTRLEN];

	pidfd = open(FLY_PID_FILE, O_WRONLY|O_CREAT|O_EXCL, S_IRUSR|S_IWUSR);
	if (pidfd == -1)
		FLY_STDERR_ERROR(
			"open pid file error."
		);

	memset(pidbuf, 0, FLY_PID_MAXSTRLEN);
	pid = fly_master_pid();
	if (pid == -1)
		return -1;
	res = snprintf(pidbuf, FLY_PID_MAXSTRLEN, "%ld", (long) pid);
	if (res < 0 || res >= FLY_PID_MAXSTRLEN)
		return -1;

	if (write(pidfd, pidbuf, strlen(pidbuf)) == -1)
		FLY_STDERR_ERROR(
			"write pid(%d) in pid file error.",
			pid
		);

	if (close(pidfd) == -1)
		FLY_STDERR_ERROR(
			"close pid file error."
		);
	return 0;
}

void fly_remove_pidfile(void)
{
	int pidfd, res;
	pid_t pid;
	char pidbuf[FLY_PID_MAXSTRLEN];

	pidfd = open(FLY_PID_FILE, O_RDONLY);
	if (pidfd == -1)
		return;

	memset(pidbuf, 0, FLY_PID_MAXSTRLEN);
	res = read(pidfd, pidbuf, FLY_PID_MAXSTRLEN);
	if (res <= 0 || res >= FLY_PID_MAXSTRLEN)
		FLY_STDERR_ERROR(
			"reading pid from pid file error in destructor."
		);

	pid = (pid_t) atol(pidbuf);
	if (pid != getpid())
		return;
	else{
		printf("Remove PID file(%ld)\n", (long) pid);
		remove(FLY_PID_FILE);
		return;
	}
}

__fly_static void __fly_master_worker_spawn(fly_master_t *master, void (*proc)(fly_context_t *, void *))
{

	if (master->req_workers <= 0)
		FLY_EMERGENCY_ERROR(
			"invalid required workers %d",
			master->req_workers
		);

	master->worker_process = proc;
	for (int i=master->now_workers;
			(i<master->req_workers && i<fly_worker_max_limit());
			i=master->now_workers){
		if (__fly_master_fork(master, WORKER, proc, master->context) == -1)
			FLY_EMERGENCY_ERROR(
				"spawn working process error."
			);
	}
}

void fly_master_worker_spawn(fly_master_t *master, void (*proc)(fly_context_t *, void *))
{
	if (!master->context || !master->context->mount)
		FLY_EMERGENCY_ERROR(
			"not found mounts info. need one or more mount points."
		);
	__fly_master_worker_spawn(master, proc);
}

__fly_static void fly_add_worker(fly_master_t *m, fly_worker_t *w)
{
	w->master = m;
	fly_bllist_add_tail(&m->workers, &w->blelem);
}

__fly_static void fly_remove_worker(fly_master_t *m, pid_t cpid)
{
	fly_worker_t *w;
	struct fly_bllist *__b;

	fly_for_each_bllist(__b, &m->workers){
		w = fly_bllist_data(__b, struct fly_worker, blelem);
		if (w->pid == cpid){
			fly_bllist_remove(&w->blelem);
			fly_free(w);
			m->now_workers--;
			return;
		}
	}
	return;
}

const char *fly_proc_type_str(fly_proc_type type)
{
	switch(type){
	case WORKER:
		return "WORKER";
	default:
		return NULL;
	}
	return NULL;
}

int __fly_master_fork(fly_master_t *master, fly_proc_type type, void (*proc)(fly_context_t *, void *), fly_context_t *ctx)
{
	pid_t pid;
	fly_worker_t *worker;

	switch((pid=fork())){
	case -1:
		return -1;
	case 0:
		{
			fly_context_t *mctx;
			/* unnecessary resource release */
			master->now_workers++;
			mctx = fly_master_release_except_context(master);

			/* alloc worker resource */
			worker = fly_worker_init(mctx);
			if (!worker)
				exit(1);

			/* set master context */
			ctx = worker->context;
		}
		break;
	default:
		/* parent */
		master->now_workers++;
		goto child_register;
	}
	/* new process only */
	proc(ctx, (void *) worker);
	exit(0);
child_register:
	switch(type){
	case WORKER:
		{
			fly_worker_t *worker = fly_malloc(sizeof(fly_worker_t));
			if (worker == NULL)
				goto error;
			worker->pid = pid;
			worker->ppid = getppid();
			fly_add_worker(master, worker);
			if (time(&worker->start) == (time_t) -1)
				goto error;

			/* spawn process notice log */
			FLY_NOTICE_DIRECT_LOG(
				master->context->log,
				"spawn %s(pid: %d). there are %d worker processes.\n",
				fly_proc_type_str(type),
				worker->pid,
				master->now_workers
			);
		}
		return 0;
	default:
		goto error;
	}
error:
	kill(pid, SIGTERM);
	FLY_EMERGENCY_ERROR(
		"try to spawn invalid process type %d",
		(int) type
	);
	return -1;

}

static int fly_master_default_fail_close(fly_event_t *e, int fd)
{
	fly_kill_workers(e->manager->ctx);
	close(fd);
	return 0;
}

__fly_static int __fly_inotify_in_mp(fly_master_t *master, fly_mount_parts_t *parts, struct inotify_event *ie)
{
	/* ie->len includes null terminate */
	int mask;
	int signum = 0;
	fly_worker_t *__w;
	struct fly_bllist *__b;

	mask = ie->mask;
	if (mask & IN_CREATE){
		signum |= FLY_SIGNAL_ADDF;
		if (fly_inotify_add_watch(parts, ie->name, ie->len-1) == -1)
			return -1;
	}
	if (mask & IN_DELETE){
		signum |= FLY_SIGNAL_DELF;
		if (fly_inotify_rm_watch(parts, ie->name, ie->len-1, mask) == -1)
			return -1;
	}
	if (mask & IN_DELETE_SELF){
		signum |= FLY_SIGNAL_UMOU;
		if (fly_inotify_rmmp(parts) == -1)
			return -1;
	}
	if (mask & IN_MOVED_FROM){
		signum |= FLY_SIGNAL_DELF;
		if (fly_inotify_rm_watch(parts, ie->name, ie->len-1, mask) == -1)
			return -1;
	}
	if (mask & IN_MOVED_TO){
		signum |= FLY_SIGNAL_ADDF;
		if (fly_inotify_add_watch(parts, ie->name, ie->len-1) == -1)
			return -1;
	}
	if (mask & IN_MOVE_SELF){
		signum |= FLY_SIGNAL_UMOU;
		if (fly_inotify_rmmp(parts) == -1)
			return -1;
	}

	fly_for_each_bllist(__b, &master->workers){
		__w = fly_bllist_data(__b, fly_worker_t, blelem);
		if (fly_send_signal(__w->pid, signum, parts->mount_number) == -1)
			return -1;
	}

	return 0;
}

__fly_static int __fly_inotify_in_pf(fly_master_t *master, struct fly_mount_parts_file *pf, struct inotify_event *ie)
{
	int mask;
	char rpath[FLY_PATH_MAX];
	fly_worker_t *__w;
	int signum = 0;

	if (fly_join_path(rpath, pf->parts->mount_path, pf->filename) == -1)
		return -1;

	mask = ie->mask;
	if (mask & IN_MODIFY){
		signum |= FLY_SIGNAL_MODF;
		if (fly_hash_update_from_parts_file_path(rpath, pf) == -1)
			return -1;
	}
	if (mask & IN_ATTRIB){
		signum |= FLY_SIGNAL_MODF;
		if (fly_hash_update_from_parts_file_path(rpath, pf) == -1)
			return -1;
	}

	struct fly_bllist *__b;
	fly_for_each_bllist(__b, &master->workers){
		__w = fly_bllist_data(__b, struct fly_worker, blelem);
		if (fly_send_signal(__w->pid, signum, pf->parts->mount_number) == -1)
			return -1;
	}
	return 0;
}

__fly_static int __fly_inotify_handle(fly_master_t *master, fly_context_t *ctx, __unused struct inotify_event *ie)
{
	int wd;
	fly_mount_parts_t *parts = NULL;
	struct fly_mount_parts_file *pf = NULL;

	wd = ie->wd;
	/* occurred in mount point directory */
	parts = fly_wd_from_parts(wd, ctx->mount);
	if (parts)
		return __fly_inotify_in_mp(master, parts, ie);

	pf = fly_wd_from_mount(wd, ctx->mount);
	if (pf)
		return __fly_inotify_in_pf(master, pf, ie);

	return 0;
}

__fly_static int __fly_master_inotify_handler(fly_event_t *e)
{
	int inofd, num_read;
	size_t inobuf_size;
	void *inobuf;
	char *__ptr;
	fly_context_t *ctx;
	fly_master_t *master;
	struct inotify_event *__e;
	fly_pool_t *pool;

	master = (fly_master_t *) e->event_data;
	ctx = master->context;
	inofd = e->fd;
	inobuf_size = FLY_NUMBER_OF_INOBUF*(sizeof(struct inotify_event) + NAME_MAX + 1);
	pool = e->manager->pool;
	inobuf = fly_pballoc(pool, inobuf_size);
	if (fly_unlikely_null(inobuf))
		return -1;

	while(true){
		num_read = read(inofd, inobuf, inobuf_size);
		if (num_read == -1){
			if (errno == EAGAIN || errno == EWOULDBLOCK)
				break;
			else
				return -1;
		}

		for (__ptr=inobuf; __ptr < (char *) (inobuf+num_read);){
			__e = (struct inotify_event *) __ptr;
			if (__fly_inotify_handle(master, ctx, __e) == -1)
				return -1;
			__ptr += sizeof(struct inotify_event) + __e->len;
		}
	}

	/* release buf */
	fly_pbfree(pool, inobuf);
	return 0;
}

__fly_static int __fly_master_inotify_event(fly_master_t *master, fly_event_manager_t *manager)
{
	fly_event_t *e;
	int inofd;
	fly_context_t *ctx = master->context;

	e = fly_event_init(manager);
	if (fly_unlikely_null(e))
		return -1;

	if (!ctx || !ctx->mount)
		return -1;

	inofd = inotify_init1(IN_CLOEXEC|IN_NONBLOCK);
	if (inofd == -1)
		return -1;

	if (fly_mount_inotify(ctx->mount, inofd) == -1)
		return -1;

	e->fd = inofd;
	e->read_or_write = FLY_READ;
	e->eflag = 0;
	fly_time_null(e->timeout);
	e->flag = FLY_PERSISTENT;
	e->tflag = FLY_INFINITY;
	e->event_data = (void *) master;
	FLY_EVENT_HANDLER(e, __fly_master_inotify_handler);
	e->expired = false;
	e->available = false;
	e->if_fail_term = true;
	e->fail_close = fly_master_default_fail_close;
	fly_event_inotify(e);

	return fly_event_register(e);
}

bool fly_is_create_pidfile(void)
{
	return fly_config_value_bool(FLY_CREATE_PIDFILE);
}

void fly_master_setreload(fly_master_t *master, const char *reload_filepath, bool configure)
{
	struct fly_watch_path *__wp;

	__wp = fly_malloc(sizeof(struct fly_watch_path));
	__wp->path = reload_filepath;
	__wp->configure = configure;
	fly_bllist_add_tail(&master->reload_filepath, &__wp->blelem);
	master->reload_pathcount++;
	master->detect_reload = true;
}


static struct fly_watch_path *__fly_search_wp(fly_master_t *__m, int wd)
{
	struct fly_bllist *__b;
	struct fly_watch_path *__wp;

	fly_for_each_bllist(__b, &__m->reload_filepath){
		__wp = (struct fly_watch_path *) fly_bllist_data(__b, struct fly_watch_path, blelem);
		if (__wp->wd == wd)
			return __wp;
	}
	return NULL;
}

/* reload fly server */
static int __fly_reload(fly_master_t *__m, struct inotify_event *__ie)
{
	int wd;
	struct fly_watch_path *__wp;

	wd = __ie->wd;
	__wp = __fly_search_wp(__m, wd);
	if (__wp == NULL)
		return -1;

	switch(__ie->mask){
	case IN_ATTRIB:
		break;
	case IN_MODIFY:
		break;
	case IN_MOVE_SELF:
		break;
	case IN_DELETE_SELF:
		break;
	default:
		FLY_NOT_COME_HERE
	}
	fly_kill_workers(__m->context);
	fly_master_release(__m);

	/* jump to master process */
#if defined HAVE_SIGLONGJMP && defined HAVE_SIGSETJMP
	siglongjmp(env, FLY_MASTER_RELOAD);
#else
	longjmp(env, FLY_MASTER_RELOAD);
#endif
}

static int __fly_master_reload_filepath_handler(fly_event_t *e)
{
#define FLY_INOTIFY_BUFSIZE(__c)			((__c)*sizeof(struct inotify_event)+NAME_MAX+1)

	fly_master_t *__m;
	char buf[FLY_INOTIFY_BUFSIZE(1)];
	ssize_t numread;
	struct inotify_event *__ie;

	__m = (fly_master_t *) e->event_data;
	/* e->fd is inotify descriptor */
	numread = read(e->fd, buf, FLY_INOTIFY_BUFSIZE(1));
	if (numread == -1){
		if (FLY_BLOCKING(numread))
			return 0;
		else{
			struct fly_err *__err;
			__err = fly_event_err_init(
				e, errno, FLY_ERR_EMERG,
				"reload file reading error."
			);
			fly_event_error_add(e, __err);
			return -1;
		}
	}

	__ie = (struct inotify_event *) buf;
	return __fly_reload(__m, __ie);
}

static int __fly_master_reload_filepath(fly_master_t *master, fly_event_manager_t *manager)
{
	if (master->reload_pathcount == 0 || !master->detect_reload)
		return 0;

	int fd;
	int wd;
	fly_event_t *e;

	fd = inotify_init1(IN_CLOEXEC|IN_NONBLOCK);
	if (fd == -1)
		return -1;

	struct fly_bllist *__b;
	struct fly_watch_path *__wp;

	fly_for_each_bllist(__b, &master->reload_filepath){
		__wp = (struct fly_watch_path *) fly_bllist_data(__b, struct fly_watch_path, blelem);
		wd = inotify_add_watch(fd, __wp->path,
			IN_CLOSE_WRITE|IN_DELETE_SELF|IN_MOVE_SELF|IN_ATTRIB|IN_IGNORED);
		if (wd == -1)
			return -1;

		__wp->wd = wd;
	}

	e = fly_event_init(manager);
	if (e == NULL)
		return -1;

	e->fd = fd;
	e->read_or_write = FLY_READ;
	e->tflag = FLY_INFINITY;
	e->eflag = 0;
	e->flag = FLY_PERSISTENT;
	e->event_fase = NULL;
	e->event_state = NULL;
	e->expired = false;
	e->available = false;
	e->event_data = (void *) master;
	e->if_fail_term = true;
	e->fail_close = fly_master_default_fail_close;
	FLY_EVENT_HANDLER(e, __fly_master_reload_filepath_handler);

	fly_time_null(e->timeout);
	fly_event_inotify(e);
	return fly_event_register(e);
}
