#include "master.h"
#include "util.h"

fly_master_t fly_master_info = {
	.req_workers = -1,
	.now_workers = 0,
	.worker_process = NULL,
	.pool = NULL,
};
fly_signal_t fly_master_signals[] = {
	{ SIGCHLD, NULL },
	{ SIGINT, NULL },
	{ SIGTERM, NULL },
};

__fly_static int __fly_get_req_workers(void);
__fly_static int __fly_master_signal_init(void);
__fly_static int __fly_master_fork(fly_proc_type type, void (*proc)(void *), void *data);
__fly_static int __fly_master_worker_spawn(void (*proc)(void *));
__fly_static int __fly_insert_workerp(fly_worker_t *w);
__fly_static fly_worker_id __fly_get_worker_id(void);
__fly_static void __fly_remove_workerp(pid_t cpid);

int fly_master_daemon(void)
{
	struct rlimit fd_limit;
	int nullfd;

	switch(fork()){
	case -1:
		return -1;
	case 0:
		break;
	default:
		exit(0);
	}

	/* child process only */
	if (setsid() == -1)
		return -1;

	/* for can't access tty */
	switch(fork()){
	case -1:
		return -1;
	case 0:
		break;
	default:
		exit(0);
	}
	/* grandchild process only */
	umask(0);
	if (chdir(FLY_ROOT_DIR) == -1)
		return -1;

	if (getrlimit(RLIMIT_NOFILE, &fd_limit) == -1)
		return -1;

	for (int i=0; i<(int) fd_limit.rlim_cur; i++){
		if (close(i) == -1 && errno != EBADF)
			return -1;
	}

	nullfd = open(__FLY_DEVNULL, 0);
	if (nullfd == -1 || nullfd != STDIN_FILENO)
		return -1;

	if (dup2(nullfd, STDOUT_FILENO) == -1)
		return -1;
	if (dup2(nullfd, STDERR_FILENO) == -1)
		return -1;
	return 0;
}

__fly_static int __fly_refresh_signal(void)
{
	for (fly_signum_t *__sig=fly_signals; *__sig>0; __sig++){
		if (*__sig!=SIGKILL && *__sig!=SIGSTOP && signal(*__sig, SIG_DFL) == SIG_ERR)
			return -1;
	}
	return 0;
}


/*
 *  adjust workers number.
 *
 */
__fly_static int __fly_workers_rebalance(int change)
{
	/* after change */
	fly_master_info.now_workers += change;
	return fly_master_worker_spawn(
			fly_master_info.worker_process
	);
}

__fly_static int __fly_sigchld(__unused siginfo_t *info)
{
	switch(info->si_code){
	case CLD_CONTINUED:
		printf("continued\n");
		goto decrement;
	case CLD_DUMPED:
		printf("dumped\n");
		goto decrement;
	case CLD_EXITED:
		printf("exited\n");
		goto decrement;
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
		return -1;
	}
decrement:
	__fly_workers_rebalance(-1);
	__fly_remove_workerp(info->si_pid);
	return 0;
}

int fly_master_waiting_for_signal(void)
{
	sigset_t master_set;
	siginfo_t master_info;
	fly_signum_t recvsig;

	if (sigemptyset(&master_set) == -1)
		return -1;

	for (int i=0; i<(int) FLY_MASTER_SIG_COUNT; i++){
		if (sigaddset(&master_set, fly_master_signals[i].number) == -1)
			return -1;
	}

	for (;;){
		recvsig = sigwaitinfo(&master_set, &master_info);
		switch(recvsig){
		case SIGCHLD:
			if (__fly_sigchld(&master_info) == -1)
				return -1;
			break;
		case SIGINT:
			return 0;
		default:
			return -1;
		}
	}
	return 0;
}

__fly_static int __fly_get_req_workers(void)
{
	char *reqworkers_env;
	reqworkers_env = getenv(FLY_WORKERS_ENV);

	if (reqworkers_env == NULL)
		return -1;

	return atoi(reqworkers_env);
}

int fly_master_init(void)
{
	fly_master_info.pid = getpid();
	fly_master_info.now_workers = 0;
	fly_master_info.req_workers = __fly_get_req_workers();
	if (fly_master_info.req_workers <= 0)
		return -1;

	if (__fly_master_signal_init() == -1)
		return -1;

	fly_master_info.pool = fly_create_pool(FLY_MASTER_POOL_SIZE);
	return 0;
}

int __fly_master_signal_init(void)
{
	if (__fly_refresh_signal() == -1)
		return -1;
	for (int i=0; i<(int) FLY_MASTER_SIG_COUNT; i++){
		struct sigaction __action;

		if (fly_master_signals[i].handler == NULL)
			__action.sa_sigaction = __fly_only_recv;
		else
			__action.sa_sigaction = fly_master_signals[i].handler;
		__action.sa_flags = SA_SIGINFO;
		if (sigaction(fly_master_signals[i].number, &__action, NULL) == -1)
			return -1;
	}
	return 0;
}

int fly_create_pidfile(void)
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

	if (write(pidfd, pidbuf, strlen(pidbuf)) == -1){
		perror("write: ");
		return -1;
	}

	if (close(pidfd) == -1)
		return -1;
	return 0;
}

__destructor int fly_remove_pidfile(void)
{
	int pidfd, res;
	pid_t pid;
	char pidbuf[FLY_PID_MAXSTRLEN];

	pidfd = open(FLY_PID_FILE, O_RDONLY);
	if (pidfd == -1)
		return -1;

	memset(pidbuf, 0, FLY_PID_MAXSTRLEN);
	res = read(pidfd, pidbuf, FLY_PID_MAXSTRLEN);
	if (res <= 0 || res >= FLY_PID_MAXSTRLEN)
		return -1;

	pid = (pid_t) atol(pidbuf);
	if (pid != getpid())
		return 0;
	else{
		printf("Remove PID file\n");
		return remove(FLY_PID_FILE);
	}
}
__fly_static int __fly_master_worker_spawn(void (*proc)(void *))
{

	if (fly_master_info.req_workers <= 0)
		return -1;

	fly_master_info.worker_process = proc;
	for (int i=fly_master_info.now_workers;
			i<fly_master_info.req_workers;
			i=fly_master_info.now_workers){
		printf("\tNow workers: %d\n", fly_master_info.now_workers);
		printf("\tReq workers: %d\n", fly_master_info.req_workers);
		fly_worker_i info;
		info.id = __fly_get_worker_id();
		info.start = time(NULL);
		if (__fly_master_fork(WORKER, proc, &info) == -1)
			return -1;
	}

	return 0;
}

int fly_master_worker_spawn(void (*proc)(void *))
{
	return __fly_master_worker_spawn(proc);
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

int __fly_master_fork(fly_proc_type type, void (*proc)(void *), void *data)
{
	pid_t pid;
	switch((pid=fork())){
	case -1:
		return -1;
	case 0:
		break;
	default:
		fly_master_info.now_workers++;
		goto child_register;
	}
	/* new process only */
	proc(data);
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
		}
		return 0;
	default:
		goto error;
	}
error:
	kill(pid, SIGTERM);
	return -1;

}

__fly_static fly_worker_id __fly_get_worker_id(void)
{
	int i=0;

	for (fly_worker_t *w=fly_master_info.workers; w!=NULL; w=w->next)
		i++;
	return i;
}
