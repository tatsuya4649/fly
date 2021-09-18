#include "master.h"
#include "util.h"
#include "cache.h"

fly_master_t fly_master_info = {
	.req_workers = -1,
	.now_workers = 0,
	.worker_process = NULL,
	.pool = NULL,
};
__fly_static int __fly_get_req_workers(void);
__fly_static int __fly_master_fork(fly_proc_type type, void (*proc)(fly_context_t *, void *), fly_context_t *ctx, void *data);
__fly_static void __fly_master_worker_spawn(void (*proc)(fly_context_t *, void *));
__fly_static int __fly_insert_workerp(fly_worker_t *w);
__fly_static fly_worker_id __fly_get_worker_id(void);
__fly_static void __fly_remove_workerp(pid_t cpid);
__fly_static int __fly_master_signal_event(fly_event_manager_t *manager, __unused fly_context_t *ctx);
__fly_static int __fly_msignal_handle(fly_context_t *ctx, struct signalfd_siginfo *info);
__fly_static int __fly_master_signal_handler(fly_event_t *);
__fly_static void __fly_workers_rebalance(fly_context_t *ctx, int change);
__fly_static void __fly_sigchld(fly_context_t *ctx, struct signalfd_siginfo *info);
__fly_static int __fly_master_inotify_event(fly_event_manager_t *manager, __unused fly_context_t *ctx);
__fly_static int __fly_master_inotify_event(fly_event_manager_t *manager, __unused fly_context_t *ctx);
__fly_static int __fly_master_inotify_handler(fly_event_t *);
#define FLY_MASTER_SIG_COUNT				(sizeof(fly_master_signals)/sizeof(fly_signal_t))

fly_signal_t fly_master_signals[] = {
	{ SIGCHLD, __fly_sigchld, NULL },
	{ SIGINT, NULL, NULL },
	{ SIGTERM, NULL, NULL },
};

int fly_master_daemon(void)
{
	struct rlimit fd_limit;
	int nullfd;

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
		if (close(i) == -1 && errno != EBADF)
			FLY_EMERGENCY_ERROR(
				FLY_EMERGENCY_STATUS_PROCS,
				"can't close file (fd: %d)", i
			);
	}

	nullfd = open(__FLY_DEVNULL, 0);
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


/*
 *  adjust workers number.
 *
 */
__fly_static void __fly_workers_rebalance(fly_context_t *ctx,int change)
{
	/* after change */
	fly_master_info.now_workers += change;
	fly_master_worker_spawn(
		ctx,
		fly_master_info.worker_process
	);
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
		/* end of worker process code */
		switch(info->ssi_status){
		case FLY_WORKER_SUCCESS_EXIT:
			goto decrement;
		case FLY_EMERGENCY_STATUS_NOMEM:
			FLY_NOTICE_DIRECT_LOG(
				fly_master_info.context->log,
				"master process(%d) detect to end of worker process(%d).  because of no memory(exit status: %d)",
				getpid(),
				info->ssi_pid,
				info->ssi_status
			);
			break;
		case FLY_EMERGENCY_STATUS_PROCS:
			FLY_NOTICE_DIRECT_LOG(
				fly_master_info.context->log,
				"master process(%d) detect to end of worker process(%d). because of process error (exit status: %d)",
				getpid(),
				info->ssi_pid,
				info->ssi_status
			);
			break;
		case FLY_EMERGENCY_STATUS_READY:
			FLY_NOTICE_DIRECT_LOG(
				fly_master_info.context->log,
				"master process(%d) detect to end of worker process(%d). because of ready error (exit status: %d)",
				getpid(),
				info->ssi_pid,
				info->ssi_status
			);
			break;
		case FLY_EMERGENCY_STATUS_ELOG:
			FLY_NOTICE_DIRECT_LOG(
				fly_master_info.context->log,
				"master process(%d) detect to end of worker process(%d). because of log error (exit status: %d)",
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
		fly_master_info.context->log,
		"worker process(pid: %d) is gone by %d(si_code).\n",
		info->ssi_pid,
		info->ssi_code
	);

	__fly_workers_rebalance(ctx, -1);
	__fly_remove_workerp(info->ssi_pid);
}

__fly_static int __fly_msignal_handle(fly_context_t *ctx, struct signalfd_siginfo *info)
{

	for (int i=0; i<(int) FLY_MASTER_SIG_COUNT; i++){
		fly_signal_t *__s = &fly_master_signals[i];
		if (__s->number == (fly_signum_t) info->ssi_signo){
			if (__s->handler)
				__s->handler(ctx, info);
			else
				fly_signal_default_handler(info);
		}
	}
	return 0;
}

__fly_static int __fly_master_signal_handler(fly_event_t *e)
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
		if (__fly_msignal_handle(e->manager->ctx, &info) == -1)
			return -1;
	}

	return 0;
}

__fly_static int __fly_master_signal_event(fly_event_manager_t *manager, __unused fly_context_t *ctx)
{
	sigset_t master_set;
	fly_event_t *e;
	int sigfd;

	if (fly_refresh_signal() == -1)
		return -1;
	if (sigemptyset(&master_set) == -1)
		return -1;

	for (int i=0; i<(int) FLY_MASTER_SIG_COUNT; i++){
		if (sigaddset(&master_set, fly_master_signals[i].number) == -1)
			return -1;
	}

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
	FLY_EVENT_HANDLER(e, __fly_master_signal_handler);

	fly_time_null(e->timeout);
	fly_event_signal(e);
	return fly_event_register(e);
}

__fly_static int __fly_get_req_workers(void)
{
	char *reqworkers_env;
	reqworkers_env = getenv(FLY_WORKERS_ENV);

	if (reqworkers_env == NULL)
		return -1;

	return atoi(reqworkers_env);
}

fly_context_t *fly_master_init(void)
{
	fly_master_info.pid = getpid();
	fly_master_info.now_workers = 0;
	fly_master_info.req_workers = __fly_get_req_workers();
	fly_master_info.context = fly_context_init();
	if (fly_master_info.req_workers <= 0)
		FLY_STDERR_ERROR(
			"Workers environment(%s) value is invalid.",
			FLY_WORKERS_ENV
		);
	if (fly_master_info.context == NULL)
		FLY_STDERR_ERROR(
			"Master context setting error."
		);

	fly_master_info.pool = fly_create_pool(FLY_MASTER_POOL_SIZE);
	if (fly_master_info.pool == NULL)
		FLY_STDERR_ERROR(
			"Making master pool error."
		);

	return fly_master_info.context;
}

__direct_log __noreturn void fly_master_process(fly_context_t *ctx)
{
	fly_event_manager_t *manager;

	manager = fly_event_manager_init(ctx);
	if (manager == NULL)
		FLY_EMERGENCY_ERROR(
			FLY_EMERGENCY_STATUS_READY,
			"master initialize event manager error."
		);

	/* initial event setting */
	if (__fly_master_signal_event(manager, ctx) == -1)
		FLY_EMERGENCY_ERROR(
			FLY_EMERGENCY_STATUS_READY,
			"initialize worker signal error."
		);
	if (__fly_master_inotify_event(manager, ctx) == -1)
		FLY_EMERGENCY_ERROR(
			FLY_EMERGENCY_STATUS_READY,
			"initialize worker inotify error."
		);

	/* event handler start here */
	if (fly_event_handler(manager) == -1)
		FLY_EMERGENCY_ERROR(
			FLY_EMERGENCY_STATUS_PROCS,
			"event handle error."
		);

	/* will not come here. */
	FLY_NOT_COME_HERE
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

__destructor int fly_remove_pidfile(void)
{
	int pidfd, res;
	pid_t pid;
	char pidbuf[FLY_PID_MAXSTRLEN];

	pidfd = open(FLY_PID_FILE, O_RDONLY);
	if (pidfd == -1)
		FLY_STDERR_ERROR(
			"open pid file error for removing in destructor."
		);

	memset(pidbuf, 0, FLY_PID_MAXSTRLEN);
	res = read(pidfd, pidbuf, FLY_PID_MAXSTRLEN);
	if (res <= 0 || res >= FLY_PID_MAXSTRLEN)
		FLY_STDERR_ERROR(
			"reading pid from pid file error in destructor."
		);

	pid = (pid_t) atol(pidbuf);
	if (pid != getpid())
		return 0;
	else{
		printf("Remove PID file(%ld)\n", (long) pid);
		return remove(FLY_PID_FILE);
	}
}

__fly_static void __fly_master_worker_spawn(void (*proc)(fly_context_t *, void *))
{

	if (fly_master_info.req_workers <= 0)
		FLY_EMERGENCY_ERROR(
			FLY_EMERGENCY_STATUS_PROCS,
			"invalid required workers %d",
			fly_master_info.req_workers
		);


	fly_master_info.worker_process = proc;
	for (int i=fly_master_info.now_workers;
			i<fly_master_info.req_workers;
			i=fly_master_info.now_workers){
		fly_worker_i info;

		info.id = __fly_get_worker_id();
		info.start = time(NULL);
		if (__fly_master_fork(WORKER, proc, fly_master_info.context, &info) == -1)
			FLY_EMERGENCY_ERROR(
				FLY_EMERGENCY_STATUS_PROCS,
				"spawn working process error."
			);
	}
}

void fly_master_worker_spawn(fly_context_t *ctx, void (*proc)(fly_context_t *, void *))
{
	if (!ctx || !ctx->mount)
		FLY_EMERGENCY_ERROR(
			FLY_EMERGENCY_STATUS_PROCS,
			"not found mounts info. need one or more mount points."
		);
	__fly_master_worker_spawn(proc);
}

__fly_static int __fly_insert_workerp(fly_worker_t *w)
{
	if (w == NULL)
		return -1;

	if (fly_master_info.workers == NULL){
		fly_master_info.workers = w;
		return 0;
	}

	fly_worker_t *i;
	for (i=fly_master_info.workers; i->next!=NULL; i=i->next)
		;

	i->next = w;
	return 0;
}

__fly_static void __fly_remove_workerp(pid_t cpid)
{
	fly_worker_t *w, *prev;

	if (cpid == fly_master_info.pid)
		return;

	for (w=fly_master_info.workers;
			w!=NULL; w=w->next){
		if (w->pid == cpid){
			if (w == fly_master_info.workers)
				fly_master_info.workers = w->next;
			else
				prev->next = w->next;
			return;
		}
		prev = w;
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

int __fly_master_fork(fly_proc_type type, void (*proc)(fly_context_t *, void *), fly_context_t *ctx, void *data)
{
	pid_t pid;
	switch((pid=fork())){
	case -1:
		return -1;
	case 0:
		break;
	default:
		/* parent */
		fly_master_info.now_workers++;
		goto child_register;
	}
	/* new process only */
	proc(ctx, data);
	exit(0);
child_register:
	switch(type){
	case WORKER:
		{
			fly_worker_t *worker = fly_pballoc(fly_master_info.pool, sizeof(fly_worker_t));
			if (worker == NULL)
				goto error;
			worker->pid = pid;
			worker->ppid = getppid();
			worker->next = NULL;

			if (__fly_insert_workerp(worker) == -1)
				goto error;

			/* spawn process notice log */
			FLY_NOTICE_DIRECT_LOG(
				fly_master_info.context->log,
				"spawn %s(pid: %d). there are %d worker processes.\n",
				fly_proc_type_str(type),
				worker->pid,
				fly_master_info.now_workers
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

__fly_static fly_worker_id __fly_get_worker_id(void)
{
	int i=0;

	for (fly_worker_t *w=fly_master_info.workers; w!=NULL; w=w->next)
		i++;
	return i;
}

__fly_static int __fly_inotify_in_mp(fly_mount_parts_t *parts, __unused struct inotify_event *ie)
{
	int mask;
	__unused int signum = 0;

	mask = ie->mask;
	if (mask & IN_CREATE){
		signum |= FLY_SIGNAL_ADDF;
		if (fly_inotify_add_watch(parts, ie->name) == -1)
			return -1;
	}
	if (mask & IN_DELETE){
		signum |= FLY_SIGNAL_DELF;
		if (fly_inotify_rm_watch(parts, ie->name, mask) == -1)
			return -1;
	}
	if (mask & IN_DELETE_SELF){
		signum |= FLY_SIGNAL_UMOU;
		if (fly_inotify_rmmp(parts) == -1)
			return -1;
	}
	if (mask & IN_MOVED_FROM){
		signum |= FLY_SIGNAL_DELF;
		if (fly_inotify_rm_watch(parts, ie->name, mask) == -1)
			return -1;
	}
	if (mask & IN_MOVED_TO){
		signum |= FLY_SIGNAL_ADDF;
		if (fly_inotify_add_watch(parts, ie->name) == -1)
			return -1;
	}
	if (mask & IN_MOVE_SELF){
		signum |= FLY_SIGNAL_UMOU;
		if (fly_inotify_rmmp(parts) == -1)
			return -1;
	}

	for (fly_worker_t *__w=fly_master_info.workers; __w; __w=__w->next){
		if (fly_send_signal(__w->pid, signum, parts->mount_number) == -1)
			return -1;
	}

	return 0;
}

__fly_static int __fly_inotify_in_pf(__unused struct fly_mount_parts_file *pf, struct inotify_event *ie)
{
	int mask;
	char rpath[FLY_PATH_MAX];

	if (fly_join_path(rpath, pf->parts->mount_path, pf->filename) == -1)
		return -1;

	mask = ie->mask;
	if (mask & IN_MODIFY){
		if (fly_hash_update_from_parts_file_path(rpath, pf) == -1)
			return -1;
	}
	if (mask & IN_ATTRIB){
		if (fly_hash_update_from_parts_file_path(rpath, pf) == -1)
			return -1;
	}
	return 0;
}

__fly_static int __fly_inotify_handle(fly_context_t *ctx, __unused struct inotify_event *ie)
{
	int wd;
	fly_mount_parts_t *parts = NULL;
	struct fly_mount_parts_file *pf = NULL;

	wd = ie->wd;
	/* occurred in mount point directory */
	parts = fly_wd_from_parts(wd, ctx->mount);
	if (parts)
		return __fly_inotify_in_mp(parts, ie);

	pf = fly_wd_from_mount(wd, ctx->mount);
	if (pf)
		return __fly_inotify_in_pf(pf, ie);

	return 0;
}

__fly_static int __fly_master_inotify_handler(fly_event_t *e)
{
	int inofd, num_read;
	size_t inobuf_size;
	void *inobuf;
	char *__ptr;
	fly_context_t *ctx;
	struct inotify_event *__e;

	ctx = (fly_context_t *) e->event_data;
	inofd = e->fd;
	inobuf_size = FLY_NUMBER_OF_INOBUF*(sizeof(struct inotify_event) + NAME_MAX + 1);
	inobuf = fly_pballoc(e->manager->pool, inobuf_size);
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
			if (__fly_inotify_handle(ctx, __e) == -1)
				return -1;
			__ptr += sizeof(struct inotify_event) + __e->len;
		}
	}

	/* TODO: release buf */
	return 0;
}

__fly_static int __fly_master_inotify_event(fly_event_manager_t *manager, fly_context_t *ctx)
{
	fly_event_t *e;
	int inofd;

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
	e->event_data = (void *) ctx;
	FLY_EVENT_HANDLER(e, __fly_master_inotify_handler);
	e->expired = false;
	e->available = false;
	fly_event_inotify(e);

	return fly_event_register(e);
}
