#include "master.h"
#include "alloc.h"
#include "util.h"
#include "cache.h"
#include "conf.h"
#include <setjmp.h>

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
#define FLY_MASTER_SIG_COUNT				(sizeof(fly_master_signals)/sizeof(fly_signal_t))
#if defined HAVE_SIGLONGJMP && defined HAVE_SIGSETJMP
static sigjmp_buf env;
#else
static jmp_buf env;
#endif

fly_signal_t fly_master_signals[] = {
	{ SIGCHLD, __fly_sigchld, NULL },
	{ SIGINT, NULL, NULL },
	{ SIGTERM, NULL, NULL },
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
			FLY_EMERGENCY_STATUS_PROCS,
			"Process(%d) must not be process group leader.",
			getpid()
		);

	/* for can't access tty */
	switch(fork()){
	case -1:
		FLY_EMERGENCY_ERROR(
			FLY_EMERGENCY_STATUS_PROCS,
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
			FLY_EMERGENCY_STATUS_PROCS,
			"can't change directory. path(%s).",
			FLY_ROOT_DIR
		);

	if (getrlimit(RLIMIT_NOFILE, &fd_limit) == -1)
		FLY_EMERGENCY_ERROR(
			FLY_EMERGENCY_STATUS_PROCS,
			"can't get resource of RLIMIT_NOFILE."
		);

	for (int i=0; i<(int) fd_limit.rlim_cur; i++){
		if (is_fly_log_fd(i, ctx))
			continue;
		if (is_fly_listen_socket(i, ctx))
			continue;

		if (close(i) == -1 && errno != EBADF)
			FLY_EMERGENCY_ERROR(
				FLY_EMERGENCY_STATUS_PROCS,
				"can't close file (fd: %d)", i
			);
	}

	nullfd = open(__FLY_DEVNULL, O_RDWR);
	if (nullfd == -1 || nullfd != STDIN_FILENO)
		FLY_EMERGENCY_ERROR(
			FLY_EMERGENCY_STATUS_PROCS,
			"can't open file (fd: %d)", nullfd
		);

	if (dup2(nullfd, STDOUT_FILENO) == -1)
		FLY_EMERGENCY_ERROR(
			FLY_EMERGENCY_STATUS_PROCS,
			"can't duplicate file %d->%d", nullfd, STDOUT_FILENO
		);
	if (dup2(nullfd, STDERR_FILENO) == -1)
		FLY_EMERGENCY_ERROR(
			FLY_EMERGENCY_STATUS_PROCS,
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
	switch(info->ssi_code){
	case CLD_CONTINUED:
		printf("continued\n");
		goto decrement;
	case CLD_DUMPED:
		printf("dumped\n");
		goto decrement;
	case CLD_EXITED:
		printf("exited\n");
		/* end status of worker */
		switch(info->ssi_status){
		case FLY_WORKER_SUCCESS_EXIT:
			goto decrement;
		case FLY_EMERGENCY_STATUS_NOMEM:
			FLY_NOTICE_DIRECT_LOG(
				ctx->log,
				"master process(%d) detect to end of worker process(%d).  because of no memory(exit status: %d)",
				getpid(),
				info->ssi_pid,
				info->ssi_status
			);
			break;
		case FLY_EMERGENCY_STATUS_PROCS:
			FLY_NOTICE_DIRECT_LOG(
				ctx->log,
				"master process(%d) detect to end of worker process(%d). because of process error (exit status: %d)",
				getpid(),
				info->ssi_pid,
				info->ssi_status
			);
			break;
		case FLY_EMERGENCY_STATUS_READY:
			FLY_NOTICE_DIRECT_LOG(
				ctx->log,
				"master process(%d) detect to end of worker process(%d). because of ready error (exit status: %d)",
				getpid(),
				info->ssi_pid,
				info->ssi_status
			);
			break;
		case FLY_EMERGENCY_STATUS_ELOG:
			FLY_NOTICE_DIRECT_LOG(
				ctx->log,
				"master process(%d) detect to end of worker process(%d). because of log error (exit status: %d)",
				getpid(),
				info->ssi_pid,
				info->ssi_status
			);
			break;
		case FLY_EMERGENCY_STATUS_NOMOUNT:
			FLY_NOTICE_DIRECT_LOG(
				ctx->log,
				"master process(%d) detect to end of worker process(%d). because of mount error (exit status: %d)",
				getpid(),
				info->ssi_pid,
				info->ssi_status
			);
			break;
		case FLY_EMERGENCY_STATUS_MODF:
			FLY_NOTICE_DIRECT_LOG(
				ctx->log,
				"master process(%d) detect to end of worker process(%d). because of modify file(hash update) error (exit status: %d)",
				getpid(),
				info->ssi_pid,
				info->ssi_status
			);
			break;
		default:
			FLY_EMERGENCY_ERROR(
				FLY_EMERGENCY_STATUS_PROCS,
				"unknown worker exit status. (%d)",
				info->ssi_status
			);
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
			FLY_EMERGENCY_STATUS_PROCS,
			"unknown signal code. (%d)",
			info->ssi_code
		);
	}

decrement:
	/* worker process is gone */
	FLY_NOTICE_DIRECT_LOG(
		ctx->log,
		"worker process(pid: %d) is gone by %d(si_code).\n",
		info->ssi_pid,
		info->ssi_code
	);

	fly_remove_worker((fly_master_t *) ctx->data, (pid_t) info->ssi_pid);
	__fly_workers_rebalance((fly_master_t *) ctx->data);
}

__noreturn static void fly_master_signal_default_handler(fly_master_t *master, fly_context_t *ctx __unused, struct signalfd_siginfo *si __unused)
{
	struct fly_bllist *__b;
	fly_worker_t *__w;

	fly_for_each_bllist(__b, &master->workers){
		__w = fly_bllist_data(__b, fly_worker_t, blelem);
		fly_send_signal(__w->pid, si->ssi_signo, 0);
	}

	fly_master_release(master);

	/* jump to master process */
#if defined HAVE_SIGLONGJMP && defined HAVE_SIGSETJMP
	siglongjmp(env, 1);
#else
	longjmp(env, 1);
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

	return __m;
}

__direct_log void fly_master_process(fly_master_t *master)
{
	fly_event_manager_t *manager;

	/* destructor setting */
	if (atexit(fly_remove_pidfile) == -1)
		FLY_EMERGENCY_ERROR(
			FLY_EMERGENCY_STATUS_READY,
			"initialize worker inotify error."
		);

	manager = fly_event_manager_init(master->context);
	if (manager == NULL)
		FLY_EMERGENCY_ERROR(
			FLY_EMERGENCY_STATUS_READY,
			"master initialize event manager error."
		);

	master->event_manager = manager;
	/* initial event setting */
	if (__fly_master_signal_event(master, manager, master->context) == -1)
		FLY_EMERGENCY_ERROR(
			FLY_EMERGENCY_STATUS_READY,
			"initialize worker signal error."
		);
	if (__fly_master_inotify_event(master, manager) == -1)
		FLY_EMERGENCY_ERROR(
			FLY_EMERGENCY_STATUS_READY,
			"initialize worker inotify error."
		);

#if defined HAVE_SIGLONGJMP && defined HAVE_SIGSETJMP
	if (sigsetjmp(env, 1) == 0){
#else
	if (setjmp(env) == 0){
#endif
		/* event handler start here */
		if (fly_event_handler(manager) == -1)
			FLY_EMERGENCY_ERROR(
				FLY_EMERGENCY_STATUS_PROCS,
				"event handle error."
			);
	}else{
		/* signal return */
		return;
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
			FLY_EMERGENCY_STATUS_PROCS,
			"invalid required workers %d",
			master->req_workers
		);

	master->worker_process = proc;
	for (int i=master->now_workers;
			i<master->req_workers;
			i=master->now_workers){
		if (__fly_master_fork(master, WORKER, proc, master->context) == -1)
			FLY_EMERGENCY_ERROR(
				FLY_EMERGENCY_STATUS_PROCS,
				"spawn working process error."
			);
	}
}

void fly_master_worker_spawn(fly_master_t *master, void (*proc)(fly_context_t *, void *))
{
	if (!master->context || !master->context->mount)
		FLY_EMERGENCY_ERROR(
			FLY_EMERGENCY_STATUS_PROCS,
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
		FLY_EMERGENCY_STATUS_PROCS,
		"try to spawn invalid process type %d",
		(int) type
	);
	return -1;

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
	fly_event_inotify(e);

	return fly_event_register(e);
}
