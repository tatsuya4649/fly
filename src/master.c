#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include "master.h"
#include "alloc.h"
#include "util.h"
#include "cache.h"
#include "conf.h"
#include <setjmp.h>
#include <signal.h>
#include "err.h"
#include <sys/wait.h>
#include <fcntl.h>

int __fly_master_fork(fly_master_t *master, fly_proc_type type, void (*proc)(fly_context_t *, void *), fly_context_t *ctx);
#ifdef HAVE_SIGNALFD
__fly_static int __fly_master_signal_event(fly_master_t *master, fly_event_manager_t *manager, __fly_unused fly_context_t *ctx);
#endif
__fly_static int __fly_msignal_handle(fly_master_t *master, fly_context_t *ctx, fly_siginfo_t *info);
__fly_static int __fly_master_signal_handler(fly_event_t *);
__fly_static void __fly_workers_rebalance(fly_master_t *master);
__fly_static void __fly_sigchld(fly_context_t *ctx, fly_siginfo_t *info);
__fly_static int __fly_master_inotify_event(fly_master_t *master, fly_event_manager_t *manager);
static int __fly_master_inotify_handler(fly_event_t *);
__fly_static void fly_add_worker(fly_master_t *m, fly_worker_t *w);
__fly_static void fly_remove_worker(fly_master_t *m, pid_t cpid);
__fly_noreturn static void fly_master_signal_default_handler(fly_master_t *master, fly_context_t *ctx __fly_unused, fly_siginfo_t *si __fly_unused);
#ifdef HAVE_INOTIFY
static int __fly_reload(fly_master_t *__m, struct inotify_event *__ie);
#elif defined HAVE_KQUEUE
static int __fly_reload(fly_master_t *__m, int fd, int mask);
#endif
static int __fly_master_reload_filepath(fly_master_t *master, fly_event_manager_t *manager);
static int __fly_master_reload_filepath_handler(fly_event_t *e);
static struct fly_watch_path *__fly_search_wp(fly_master_t *__m, int wd);
static void __fly_master_set_orig_sighandler(fly_master_t *__m);
static int __fly_master_get_now_sighandler(fly_master_t *__m, struct fly_err *err);
static int fly_master_default_fail_close(fly_event_t *e, int fd);
void fly_master_notice_worker_daemon_pid(fly_context_t *ctx, fly_siginfo_t *info);
static void __fly_master_release_orig_sighandler(fly_master_t *__m);
#if defined DEBUG && defined HAVE_KQUEUE
static void fly_master_mount_parts_debug(fly_master_t *master, fly_event_manager_t *__m, fly_event_t *event);
#endif
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
	FLY_SIGNAL_SETTING(SIGWINCH, FLY_SIG_IGN),
#ifdef HAVE_SIGQUEUE
	FLY_SIGNAL_SETTING(SIGUSR1, fly_master_notice_worker_daemon_pid),
#else
	FLY_SIGNAL_SETTING(SIGUSR1, FLY_SIG_IGN),
#endif
};


static long fly_worker_max_limit(void)
{
	return fly_config_value_long(FLY_WORKER_MAX);
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

__fly_static void __fly_sigchld(fly_context_t *ctx, fly_siginfo_t *info)
{
	fly_master_t *master;

	master = (fly_master_t *) ctx->data;

	/* receive */
#ifdef HAVE_SIGNALFD
	if (waitpid(info->ssi_pid, NULL, WNOHANG) == -1)
#else
	if (waitpid(info->si_pid, NULL, WNOHANG) == -1)
#endif
		FLY_EMERGENCY_ERROR(
			"master waitpid error. (%s: %d)",
			__FILE__, __LINE__
		);

#ifdef HAVE_SIGNALFD
	switch(info->ssi_code){
#else
	switch(info->si_code){
#endif
	case CLD_CONTINUED:
		printf("continued\n");
		goto decrement;
	case CLD_DUMPED:
		FLY_NOTICE_DIRECT_LOG(
			ctx->log,
			"master process(%d) detected the dumped of worker process(%d).\n",
			getpid(),
#ifdef HAVE_SIGNALFD
			info->ssi_pid,
			info->ssi_status
#else
			info->si_pid,
			info->si_status
#endif
		);
		goto decrement;
	case CLD_EXITED:
		/* end status of worker(error level) */
#ifdef HAVE_SIGNALFD
		switch(info->ssi_status){
#else
		switch(info->si_status){
#endif
		case FLY_WORKER_SUCCESS_EXIT:
			goto decrement;
		case FLY_ERR_EMERG:
			FLY_NOTICE_DIRECT_LOG(
				ctx->log,
				"master process(%d) detected the emergency termination of worker process(%d).\n",
				getpid(),
#ifdef HAVE_SIGNALFD
				info->ssi_pid,
				info->ssi_status
#else
				info->si_pid,
				info->si_status
#endif
			);
			/* terminate fly processes. */
			goto fly_terminate;
		case FLY_ERR_CRIT:
			FLY_NOTICE_DIRECT_LOG(
				ctx->log,
				"master process(%d) detected the critical termination of worker process(%d).\n",
				getpid(),
#ifdef HAVE_SIGNALFD
				info->ssi_pid,
				info->ssi_status
#else
				info->si_pid,
				info->si_status
#endif
			);
			/* terminate fly processes. */
			goto fly_terminate;
		case FLY_ERR_ERR:
			FLY_NOTICE_DIRECT_LOG(
				ctx->log,
				"master process(%d) detected the error termination of worker process(%d).\n",
				getpid(),
#ifdef HAVE_SIGNALFD
				info->ssi_pid,
				info->ssi_status
#else
				info->si_pid,
				info->si_status
#endif
			);

			goto decrement;
		default:
#ifndef DEBUG
			assert(0);
#endif
			FLY_EMERGENCY_ERROR(
				"unknown worker exit status. (%d)",
#ifdef HAVE_SIGNALFD
				info->ssi_status
#else
				info->si_status
#endif
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
#ifdef HAVE_SIGNALFD
			info->ssi_code
#else
			info->si_code
#endif
		);
	}

decrement:
#ifdef HAVE_SIGNALFD
	fly_remove_worker((fly_master_t *) ctx->data, (pid_t) info->ssi_pid);
#else
	fly_remove_worker((fly_master_t *) ctx->data, (pid_t) info->si_pid);
#endif
	fly_notice_direct_log(
		ctx->log,
		"Master: Detected the terminated of Worker(%d). Create a new worker.\n",
#ifdef HAVE_SIGNALFD
		info->ssi_pid,
#else
		info->si_pid,
#endif
		master->pid
	);

	__fly_workers_rebalance(master);
	return;

fly_terminate:
	/* fly all processes(workers/master) terminate */
	fly_master_signal_default_handler(master, ctx, info);
	return;
}

__fly_noreturn static void fly_master_signal_default_handler(fly_master_t *master, fly_context_t *ctx __fly_unused, fly_siginfo_t *si __fly_unused)
{
	struct fly_bllist *__b;
	fly_worker_t *__w;

	fly_notice_direct_log(
		ctx->log,
		"Master: Process(%d) is received signal(%s). kill workers.\n",
		master->pid,
#ifdef HAVE_SIGNALFD
		strsignal(si->ssi_signo)
#else
		strsignal(si->si_signo)
#endif
	);
#ifdef DEBUG
	printf("MASTER SIGNAL DEFAULT HANDLER\n");
#endif
retry:
	fly_for_each_bllist(__b, &master->workers){
		__w = fly_bllist_data(__b, fly_worker_t, blelem);
		fly_send_signal(__w->pid, SIGTERM, 0);

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

__fly_static int __fly_msignal_handle(fly_master_t *master, fly_context_t *ctx, fly_siginfo_t *info)
{
	struct fly_signal *__s;
	struct fly_bllist *__b;

#ifdef DEBUG
#ifdef HAVE_SIGNALFD
	printf("MASTER: SIGNAL RECEIVED (%s)\n", strsignal(info->ssi_signo));
#else
	printf("MASTER: SIGNAL RECEIVED (%s)\n", strsignal(info->si_signo));
#endif
#endif
	fly_for_each_bllist(__b, &master->signals){
		__s = fly_bllist_data(__b, struct fly_signal, blelem);
#ifdef HAVE_SIGNALFD
		if (__s->number == (fly_signum_t) info->ssi_signo){
#else
		if (__s->number == (fly_signum_t) info->si_signo){
#endif
			if (__s->handler)
				__s->handler(ctx, info);
			else
				fly_master_signal_default_handler(master, ctx, info);

			return 0;
		}
	}

	fly_master_signal_default_handler(master, ctx, info);
	return 0;
}

static void fly_add_master_sig(fly_context_t *ctx, int num, fly_sighand_t *handler)
{
	fly_master_t *__m;
	fly_signal_t *__nf;

	__m = (fly_master_t *) ctx->data;
	__nf = fly_pballoc(ctx->pool, sizeof(struct fly_signal));
	__nf->number = num;
	__nf->handler = handler;
	fly_bllist_add_tail(&__m->signals, &__nf->blelem);
}

void fly_master_notice_worker_daemon_pid(fly_context_t *ctx, fly_siginfo_t *info)
{
	fly_master_t *__m;
	fly_worker_t *__w;
	struct fly_bllist *__b;
	pid_t orig_worker_pid=0;

	__m = (fly_master_t *) ctx->data;
#ifdef HAVE_SIGNALFD
	orig_worker_pid = info->ssi_int;
#elif defined HAVE_SIGQUEUE
	orig_worker_pid = info->si_value.sival_int;
#endif

	fly_notice_direct_log(
		ctx->log,
		"master process(%d) is received signal(%s). notice worker pid. (%d->%d)\n",
		__m->pid,
#ifdef HAVE_SIGNALFD
		strsignal(info->ssi_signo),
#else
		strsignal(info->si_signo),
#endif
		orig_worker_pid,
#ifdef HAVE_SIGNALFD
		info->ssi_pid
#else
		info->si_pid
#endif
	);

	fly_for_each_bllist(__b, &__m->workers){
		__w = fly_bllist_data(__b, fly_worker_t, blelem);
		if (__w->pid == orig_worker_pid){
			/* update worker daemon process id */
#ifdef HAVE_SIGNALFD
			__w->pid = info->ssi_pid;
#else
			__w->pid = info->si_pid;
#endif
			break;
		}else
			continue;
	}
}

//static void fly_master_rtsignal_added(fly_context_t *ctx)
//{
//	fly_add_master_sig(ctx, FLY_NOTICE_WORKER_DAEMON_PID, fly_master_notice_worker_daemon_pid);
//}

__fly_unused __fly_static int __fly_master_signal_handler(fly_event_t *e)
{
	fly_siginfo_t info;

	ssize_t res;
	while(true){
		res = read(e->fd, (void *) &info,sizeof(fly_siginfo_t));
		if (res == -1){
			if (errno == EAGAIN || errno == EWOULDBLOCK)
				break;
			else
				return -1;
		}
		if (__fly_msignal_handle((fly_master_t *) fly_event_data_get(e, __p), e->manager->ctx, &info) == -1)
			return -1;
	}

	return 0;
}

#ifdef HAVE_SIGNALFD

static int __fly_master_signal_end_handler(fly_event_t *__e)
{
	return close(__e->fd);
}

__fly_static int __fly_master_signal_event(fly_master_t *master, fly_event_manager_t *manager, __fly_unused fly_context_t *ctx)
{
	sigset_t master_set;
	fly_event_t *e;

	if (fly_refresh_signal() == -1)
		return -1;

	for (int i=0; i<(int) FLY_MASTER_SIG_COUNT; i++)
		fly_add_master_sig(ctx, fly_master_signals[i].number, fly_master_signals[i].handler);
//	fly_master_rtsignal_added(ctx);

	if (sigfillset(&master_set) == -1)
		return -1;
	int sigfd;
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
	e->expired = false;
	e->available = false;
	//e->event_data = (void *) master;
	fly_event_data_set(e, __p, (void *) master);
	e->if_fail_term = true;
	e->fail_close = fly_master_default_fail_close;
	e->end_handler = __fly_master_signal_end_handler;
	FLY_EVENT_HANDLER(e, __fly_master_signal_handler);
	fly_time_null(e->timeout);
	fly_event_signal(e);

	return fly_event_register(e);
}
#else
static fly_master_t *__mptr;

static void __fly_master_sigaction(int signum __fly_unused, fly_siginfo_t *info, void *ucontext __fly_unused)
{
	__fly_msignal_handle(__mptr, __mptr->context, info);
}

__fly_static int __fly_master_signal(fly_master_t *master, fly_event_manager_t *manager __fly_unused, __fly_unused fly_context_t *ctx)
{
#define FLY_KQUEUE_MASTER_SIGNALSET(signum)						\
		do{													\
			struct sigaction __sa;							\
			memset(&__sa, '\0', sizeof(struct sigaction));	\
			if (sigfillset(&__sa.sa_mask) == -1)			\
				return -1;									\
			__sa.sa_sigaction = __fly_master_sigaction;		\
			__sa.sa_flags = SA_SIGINFO;						\
			if (sigaction((signum), &__sa, NULL) == -1)		\
				return -1;									\
		} while(0)

	if (fly_refresh_signal() == -1)
		return -1;

	for (int i=0; i<(int) FLY_MASTER_SIG_COUNT; i++)
		fly_add_master_sig(ctx, fly_master_signals[i].number, fly_master_signals[i].handler);

	__mptr = master;
	FLY_KQUEUE_MASTER_SIGNALSET(SIGABRT);
	FLY_KQUEUE_MASTER_SIGNALSET(SIGALRM);
	FLY_KQUEUE_MASTER_SIGNALSET(SIGBUS);
	FLY_KQUEUE_MASTER_SIGNALSET(SIGCHLD);
	FLY_KQUEUE_MASTER_SIGNALSET(SIGCONT);
	FLY_KQUEUE_MASTER_SIGNALSET(SIGFPE);
	FLY_KQUEUE_MASTER_SIGNALSET(SIGHUP);
	FLY_KQUEUE_MASTER_SIGNALSET(SIGILL);
	FLY_KQUEUE_MASTER_SIGNALSET(SIGINFO);
	FLY_KQUEUE_MASTER_SIGNALSET(SIGINT);
	FLY_KQUEUE_MASTER_SIGNALSET(SIGPIPE);
#ifdef SIGPOLL
	FLY_KQUEUE_MASTER_SIGNALSET(SIGPOLL);
#endif
	FLY_KQUEUE_MASTER_SIGNALSET(SIGPROF);
	FLY_KQUEUE_MASTER_SIGNALSET(SIGQUIT);
	FLY_KQUEUE_MASTER_SIGNALSET(SIGSEGV);
	FLY_KQUEUE_MASTER_SIGNALSET(SIGTSTP);
	FLY_KQUEUE_MASTER_SIGNALSET(SIGSYS);
	FLY_KQUEUE_MASTER_SIGNALSET(SIGTERM);
	FLY_KQUEUE_MASTER_SIGNALSET(SIGTRAP);
	FLY_KQUEUE_MASTER_SIGNALSET(SIGTTIN);
	FLY_KQUEUE_MASTER_SIGNALSET(SIGTTOU);
	FLY_KQUEUE_MASTER_SIGNALSET(SIGURG);
	FLY_KQUEUE_MASTER_SIGNALSET(SIGUSR1);
	FLY_KQUEUE_MASTER_SIGNALSET(SIGUSR2);
	FLY_KQUEUE_MASTER_SIGNALSET(SIGVTALRM);
	FLY_KQUEUE_MASTER_SIGNALSET(SIGXCPU);
	FLY_KQUEUE_MASTER_SIGNALSET(SIGXFSZ);
#ifdef SIGWINCH
	FLY_KQUEUE_MASTER_SIGNALSET(SIGWINCH);
#endif

#if defined SIGRTMIN && defined SIGRTMAX
	for (int i=SIGRTMIN; i<SIGRTMAX; i++)
		FLY_KQUEUE_MASTER_SIGNALSET(i);
#endif

	sigset_t sset;
	if (sigfillset(&sset) == -1)
		return -1;
	if (sigprocmask(SIG_UNBLOCK, &sset, NULL) == -1)
		return -1;

	return 0;
}
#endif

static long fly_workers_count(void)
{
	return fly_config_value_long(FLY_WORKER);
}

void fly_master_release(fly_master_t *master)
{
#ifdef DEBUG
	printf("MASTER: ==================release resource==================\n");
	printf("MASTER: now event count %d: %d\n", master->event_manager->monitorable.count, master->event_manager->unmonitorable.count);
	fflush(stdout);
#endif
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
	__fly_master_set_orig_sighandler(master);
	fly_free(master);
#ifdef DEBUG
	printf("MASTER: ====================================================\n");
	fflush(stdout);
#endif
}

fly_context_t *fly_master_release_except_context(fly_master_t *master)
{
	assert(master != NULL);

	fly_context_t *ctx = master->context;
	ctx->pool_manager = NULL;

	__fly_master_release_orig_sighandler(master);
	if (master->event_manager)
		fly_event_manager_release(master->event_manager);
	fly_pool_manager_release(master->pool_manager);
	fly_free(master);

	return ctx;
}

static void __fly_master_release_orig_sighandler(fly_master_t *__m)
{
	struct fly_bllist *__b, *__n;
	struct fly_orig_signal *__s;
	for (__b=__m->orig_sighandler.next; __b!=&__m->orig_sighandler; __b=__n){
		__n = __b->next;
		__s = fly_bllist_data(__b, struct fly_orig_signal, blelem);
		fly_free(__s);
	}
	return;
}

static int __fly_master_get_now_sighandler(fly_master_t *__m, struct fly_err *err)
{
	for (fly_signum_t *__a=fly_signals; *__a!=-1; __a++){
		if (*__a == SIGKILL || *__a == SIGSTOP)
			continue;

		struct sigaction __osa;
		struct fly_orig_signal *__s;
		if (sigaction(*__a, NULL, &__osa) == -1){
			fly_error(
				err,
				errno,
				FLY_ERR_ERR,
				"Signal handler setting error. %s (%s: %d)",
				strerror(errno), __FILE__, __LINE__
			);
			return -1;
		}

		__s = fly_malloc(sizeof(struct fly_orig_signal));
		__s->number = *__a;
		memcpy(&__s->sa, &__osa, sizeof(struct sigaction));
		fly_bllist_add_tail(&__m->orig_sighandler, &__s->blelem);
		__m->sigcount++;
	}
	return 0;
}

static void __fly_master_set_orig_sighandler(fly_master_t *__m)
{
	struct fly_bllist *__b, *__n;
	struct fly_orig_signal *__s;

retry:
	for (__b=__m->orig_sighandler.next; __b!=&__m->orig_sighandler; __b=__n)
	{
		__n = __b->next;
		__s = (struct fly_orig_signal *) fly_bllist_data(__b, struct fly_orig_signal, blelem);
		if (__s->number != SIGKILL && __s->number != SIGSTOP){
			if (sigaction(__s->number, &__s->sa, NULL) == -1)
				FLY_EMERGENCY_ERROR(
					"master setting original signal handler error. (%s: %d)",
					__FILE__, __LINE__
				);
		}

		fly_bllist_remove(&__s->blelem);
		fly_free(__s);
		__m->sigcount--;

		goto retry;
	}
#ifdef DEBUG
	assert(__m->sigcount == 0);
#endif
	sigset_t set;

	if (sigfillset(&set) == -1)
		FLY_EMERGENCY_ERROR(
			"master release sigfillset error. (%s: %d)",
			__FILE__, __LINE__
		);
	if (sigprocmask(SIG_UNBLOCK, &set, NULL) == -1)
		FLY_EMERGENCY_ERROR(
			"master release sigprocmask error. (%s: %d)",
			__FILE__, __LINE__
		);

	return;
}

fly_master_t *fly_master_init(struct fly_err *err)
{
	struct fly_pool_manager *__pm;
	fly_master_t *__m;
	fly_context_t *__ctx;

	__pm = fly_pool_manager_init();
	__m = fly_malloc(sizeof(fly_master_t));
	__ctx = fly_context_init(__pm, err);
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
	__m->sigcount = 0;
	fly_bllist_init(&__m->signals);
	fly_bllist_init(&__m->orig_sighandler);
	if (__fly_master_get_now_sighandler(__m, err) == -1)
		return NULL;

	setvbuf(stdout, NULL, _IONBF, 0);
	setvbuf(stderr, NULL, _IONBF, 0);
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
		/* kill worker */
		fly_send_signal(__w->pid, SIGTERM, 0);

		if (wait(NULL) == -1)
			FLY_EMERGENCY_ERROR(
				"master process wait error. (%s: %d)",
				__FILE__, __LINE__
			);

		fly_remove_worker(master, __w->pid);
		goto retry;
	}
	master->now_workers = 0;
}

__fly_direct_log int fly_master_process(fly_master_t *master)
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
#ifdef HAVE_SIGNALFD
	if (__fly_master_signal_event(master, manager, master->context) == -1)
		FLY_EMERGENCY_ERROR(
			"initialize master signal error. %s",
			strerror(errno)
		);
#else
	if (__fly_master_signal(master, manager, master->context) == -1)
		FLY_EMERGENCY_ERROR(
			"initialize master signal error. %s",
			strerror(errno)
		);
#endif
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
#ifdef DEBUG
		printf("MASTER PROCESS EVENT START!\n");
#endif
		switch(fly_event_handler(manager)){
		case FLY_EVENT_HANDLER_INVALID_MANAGER:
			FLY_EMERGENCY_ERROR(
				"event handle error. \
				event manager is invalid. (%s: %d)",
				__FILE__, __LINE__
			);
			break;
		case FLY_EVENT_HANDLER_EPOLL_ERROR:
			FLY_EMERGENCY_ERROR(
				"event handle error. \
				epoll was broken. (%s: %d)",
				__FILE__, __LINE__
			);
			break;
		case FLY_EVENT_HANDLER_EXPIRED_EVENT_ERROR:
			FLY_EMERGENCY_ERROR(
				"event handle error. \
				occurred error in expired event handler. (%s: %d)",
				__FILE__, __LINE__
			);
			break;
		default:
			FLY_NOT_COME_HERE
		}
	}else if (res == FLY_MASTER_SIGNAL_END){
#ifdef DEBUG
		printf("MASTER PROCESS SIGNAL END\n");
#endif
		/* signal or reload file return */
		return res;
	}else if (res == FLY_MASTER_RELOAD){
#ifdef DEBUG
		printf("MASTER PROCESS RELOAD\n");
#endif
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
	sigset_t __s;

	/* block signals */
	if (sigfillset(&__s) == -1)
		return -1;
	if (sigprocmask(SIG_BLOCK, &__s, NULL) == -1)
		return -1;

	pid = fork();
	if (pid == -1)
		return -1;
	else if (pid == 0){
		fly_context_t *mctx=NULL;
		/* unnecessary resource release */
		master->now_workers++;
		mctx = fly_master_release_except_context(master);

		/* alloc worker resource */
		worker = fly_worker_init(mctx);
		if (!worker)
			exit(1);

		/* set master context */
		ctx = worker->context;
#ifdef HAVE_SIGNALFD
		if (sigprocmask(SIG_UNBLOCK, &__s, NULL) == -1)
			return -1;
#endif
	}else{
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
			worker->orig_pid = pid;
			worker->master_pid = getpid();
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

#ifdef HAVE_INOTIFY
__fly_static int __fly_inotify_in_mp(fly_master_t *master, fly_mount_parts_t *parts, struct inotify_event *ie)
{
	/* ie->len includes null terminate */
	int mask;
	fly_worker_t *__w;
	struct fly_bllist *__b;

	mask = ie->mask;
	if (mask & IN_CREATE){
#ifdef DEBUG
		printf("MASTER: New file is created at mount point.\n");
#endif
		if (fly_inotify_add_watch(parts, ie->name, ie->len-1) == -1)
			return -1;
		FLY_NOTICE_DIRECT_LOG(master->context->log,
				"Master detected a created file/directory(%s) at mount point(%s).\n",
				ie->name, parts->mount_path);
	}
	if (mask & IN_DELETE_SELF){
#ifdef DEBUG
		printf("MASTER: Mount point is deleted.\n");
#endif
		parts->deleted = true;
		if (fly_inotify_rmmp(parts) == -1)
			return -1;

		FLY_NOTICE_DIRECT_LOG(master->context->log,
				"Master detected a deleted file/directory(%s) at mount point(%s).\n",
				ie->name, parts->mount_path);
	}
	if (mask & IN_MOVED_TO){
#ifdef DEBUG
		printf("MASTER: New file is move to mount point.\n");
#endif
		if (fly_inotify_add_watch(parts, ie->name, ie->len-1) == -1)
			return -1;

		FLY_NOTICE_DIRECT_LOG(master->context->log,
				"Master detected a moved file/directory(%s) to mount point(%s).\n",
				ie->name, parts->mount_path);
	}
	if (mask & IN_MOVE_SELF){
#ifdef DEBUG
		printf("MASTER: Mount point is moved.\n");
#endif
		if (fly_inotify_rmmp(parts) == -1)
			return -1;

		FLY_NOTICE_DIRECT_LOG(master->context->log,
				"Master detected a moved mount point(%s).\n",
				ie->name, parts->mount_path);
	}

	fly_for_each_bllist(__b, &master->workers){
		__w = fly_bllist_data(__b, fly_worker_t, blelem);
		if (fly_send_signal(__w->pid, FLY_SIGNAL_CHANGE_MNT_CONTENT, FLY_UNUSE_SIGNAL_VALUE) == -1)
			return -1;
	}

	return 0;
}
#elif defined HAVE_KQUEUE
__fly_static int __fly_inotify_in_mp(fly_master_t *master, fly_mount_parts_t *parts, fly_event_t *event)
{
	fly_worker_t *__w;
	struct fly_bllist *__b;
	int mask;
#ifdef DEBUG
	int __tmp, __tmpc;
	size_t __tmpm;
	fly_mount_t *__m;
	__tmp = parts->file_count;
	__tmpc = parts->mount->mount_count;
	__tmpm = parts->mount->file_count;
	__m = parts->mount;
#endif

	mask = event->eflag;
	FLY_NOTICE_DIRECT_LOG(master->context->log,
			"Master detected change at mount point(%s).\n",
			parts->mount_path);
#ifdef DEBUG
	printf("MASTER: INOTIFY at mount point. mask is \"0x%x\"\n", mask);
#endif
	/* create new file */
	if ((mask & NOTE_EXTEND) || (mask & NOTE_WRITE)){
		if (fly_inotify_add_watch(parts, event) == -1)
			return -1;
#ifdef  DEBUG
		if (__tmp == parts->file_count)
			printf("MASTER: Detect not added file/directory.\n");
		else
			printf("MASTER: Detect added file/directory.\n");
#endif
	}
	if ((mask & NOTE_RENAME) || (mask & NOTE_DELETE)){
		fly_event_t *__e;
		if (mask & NOTE_DELETE)
			parts->deleted = true;
#ifdef DEBUG
		printf("MASTER: Detect rename/detect of mount point.\n");
		printf("MASTER: Unmount mount point. %s\n", parts->mount_path);
		printf("\tUnmount file count %d\n", __tmp);
		printf("\tMount point count %d --> %d\n", __tmpc, __m->mount_count);
		printf("\tTotal file count %ld --> %ld\n", __tmpm, __m->file_count);
#endif
		__e = parts->event;
		if (fly_inotify_rmmp(parts) == -1)
			return -1;

		if (__e->fd != event->fd && \
				fly_event_unregister(__e) == -1)
			return -1;
	}

	FLY_NOTICE_DIRECT_LOG(master->context->log,
			"Master send signal to worker to tell changed mount content.\n"
			);
	fly_for_each_bllist(__b, &master->workers){
		__w = fly_bllist_data(__b, fly_worker_t, blelem);
#ifdef DEBUG
		printf("MASTER: Send signal to worker(\"%d\") to notificate chnaged content of mount point\n", __w->pid);
#endif
		if (fly_send_signal(__w->pid, FLY_SIGNAL_CHANGE_MNT_CONTENT, 0) == -1)
			return -1;
	}

	return 0;
}
#endif

static int __fly_file_deleted(const char *path)
{
	struct stat st;
	if (stat(path, &st) == -1)
		return 1;
	else
		return 0;
}


#ifdef HAVE_INOTIFY
__fly_static int __fly_inotify_in_pf(fly_master_t *master, struct fly_mount_parts_file *pf, struct inotify_event *ie)
{
	int mask;
	char rpath[FLY_PATH_MAX];
	fly_worker_t *__w;
	int res;

	res = snprintf(rpath, FLY_PATH_MAX, "%s/%s", pf->parts->mount_path, pf->filename);
	if (res < 0 || res == FLY_PATH_MAX)
		return -1;

#ifdef DEBUG
	printf("MASTER: Detected at %s(%s)\n", rpath, pf->dir ? "dir": "file");
#endif
	mask = ie->mask;
	if (mask & IN_DELETE_SELF){
		pf->deleted = true;
		goto deleted;
	}
	if (mask & IN_MOVE_SELF)
		goto deleted;
	if (pf->dir){
		if ((mask & IN_CREATE) || (mask & IN_MOVED_TO)){
			char rpath_from_mp[FLY_PATH_MAX], *rptr;
			memset(rpath_from_mp, '\0', FLY_PATH_MAX);
			rptr = rpath_from_mp;
#ifdef DEBUG
			if (mask & IN_CREATE)
				printf("MASTER: Detect added file(create). %s\n", ie->name);
			else
				printf("MASTER: Detect added file(moved). %s\n", ie->name);
#endif
			memcpy(rptr, pf->filename, pf->filename_len);
			*(rptr+pf->filename_len) = '/';
			memcpy(rptr+pf->filename_len+1, ie->name, ie->len);
			if (fly_inotify_add_watch(pf->parts, rptr, strlen(rptr)) == -1)
				return -1;

			FLY_NOTICE_DIRECT_LOG(master->context->log,
					"Master detected a new directory(%s).\n",
					rpath);
		}
	}else{
		if (mask & IN_MODIFY){
			if (fly_hash_update_from_parts_file_path(rpath, pf) == -1)
				return -1;
			FLY_NOTICE_DIRECT_LOG(master->context->log,
					"Master detected modification of a directory.\n",
					rpath);
		}
		if (mask & IN_ATTRIB){
			/*
			 * If the file is deleted, IN_ATTRIB will occur which means that link count of file has changed.
			 */
			if (__fly_file_deleted((const char *) rpath)){
				pf->deleted = true;
				goto deleted;
			}
			if (fly_hash_update_from_parts_file_path(rpath, pf) == -1){
				return -1;
			}
			FLY_NOTICE_DIRECT_LOG(master->context->log,
					"Master detected modification of metadata of a directory.\n",
					rpath);
		}
	}

	goto send_signal;
deleted:
#ifdef DEBUG
	printf("MASTER: Detect renamed/deleted file. Remove watch(%s). %s\n", rpath, pf->deleted ? "DELETE" : "ALIVE");
#endif
	FLY_NOTICE_DIRECT_LOG(master->context->log,
			"Master detected a deleted %s(%s).\n",
			pf->dir ? "directory" : "file", rpath);
	printf("SUCCESS DEBUG!!!\n");
	if (fly_inotify_rm_watch(pf) == -1)
		return -1;

send_signal:
	;
	struct fly_bllist *__b;
	fly_for_each_bllist(__b, &master->workers){
		__w = fly_bllist_data(__b, struct fly_worker, blelem);
#ifdef DEBUG
		printf("MASTER: Send signal to worker(\"%d\") to notificate chnaged content of mount point\n", __w->pid);
#endif
		if (fly_send_signal(__w->pid, FLY_SIGNAL_CHANGE_MNT_CONTENT, 0) == -1)
			return -1;
	}
	return 0;
}
#elif defined HAVE_KQUEUE

__fly_static int __fly_inotify_in_pf(fly_master_t *master, struct fly_mount_parts_file *pf, struct fly_event *event, int flag)
{
	int mask;
	char rpath[FLY_PATH_MAX];
	fly_worker_t *__w;
	int res;
	struct fly_mount_parts *parts;

	parts = pf->parts;
#ifdef DEBUG
	assert(parts != NULL);
#endif
	res = snprintf(rpath, FLY_PATH_MAX, "%s/%s", parts->mount_path, pf->filename);
	if (res < 0 || res == FLY_PATH_MAX)
		return -1;

	mask = flag;
	if (pf->dir){
		int __tmp;
		__tmp = parts->file_count;

		if ((mask & NOTE_EXTEND) || (mask & NOTE_WRITE)){
			if (fly_inotify_add_watch(parts, pf->event) == -1)
				return -1;
#ifdef  DEBUG
			if (__tmp == parts->file_count)
				printf("MASTER: Detect not added file.\n");
			else
				printf("MASTER: Detect added file.\n");
#endif
			FLY_NOTICE_DIRECT_LOG(master->context->log,
					"Master detected that the number of files to be watched at directory has %s (%s).\n",
					__tmp == parts->file_count ? "not added" : "added", rpath);
		}
		if ((mask & NOTE_RENAME) || (mask & NOTE_DELETE)){
			fly_event_t *__e;

			__e = pf->event;
			if (mask & NOTE_DELETE)
				pf->deleted = true;
			if (fly_inotify_rm_watch(pf) == -1)
				return -1;
			if (__e->fd != event->fd && \
					fly_event_unregister(__e) == -1)
				return -1;
#ifdef DEBUG
			printf("MASTER: Detect rename/detect of mount point.\n");
#endif
			FLY_NOTICE_DIRECT_LOG(master->context->log,
					"Master detected a %s directory(%s).\n",
					mask & NOTE_RENAME ? "renamed" : "deleted",
					rpath);
		}
		goto send_signal;
	}else{
		if (mask & NOTE_DELETE){
			pf->deleted = true;
			goto deleted;
		}
		if (mask & NOTE_WRITE){
			if (fly_hash_update_from_parts_file_path(rpath, pf) == -1)
				return -1;
		}
		if (mask & NOTE_EXTEND){
			if (fly_hash_update_from_parts_file_path(rpath, pf) == -1)
				return -1;
		}
		if (mask & NOTE_ATTRIB){
			if (fly_hash_update_from_parts_file_path(rpath, pf) == -1)
				return -1;
		}
		if (mask & NOTE_RENAME)
			goto deleted;
	}

	if (__fly_file_deleted((const char *) rpath)){
		pf->deleted = true;
		goto deleted;
	}

	goto send_signal;
deleted:
	;
	fly_event_t *__e;
#ifdef DEBUG
	printf("MASTER: Detect renamed/deleted file. Remove watch(%s)\n", rpath);
#endif
	__e = pf->event;
	FLY_NOTICE_DIRECT_LOG(master->context->log,
			"Master detected a deleted %s(%s),\n",
			pf->dir ? "directory" : "file", rpath);
	if (fly_inotify_rm_watch(pf) == -1)
		return -1;
	if (__e->fd != event->fd && \
			fly_event_unregister(__e) == -1)
		return -1;
send_signal:
	;

	FLY_NOTICE_DIRECT_LOG(master->context->log,
			"Master send signal to worker to tell changed mount content.\n"
			);
	struct fly_bllist *__b;
	fly_for_each_bllist(__b, &master->workers){
		__w = fly_bllist_data(__b, struct fly_worker, blelem);
#ifdef DEBUG
		printf("MASTER: Send signal to worker(\"%d\") to notificate chnaged content of mount point\n", __w->pid);
#endif
		if (fly_send_signal(__w->pid, FLY_SIGNAL_CHANGE_MNT_CONTENT, 0) == -1){
#ifdef DEBUG
			perror("fly_send_signal");
#endif
			return -1;
		}
	}
	return 0;
}
#endif

#ifdef HAVE_INOTIFY
__fly_static int __fly_inotify_handle(fly_master_t *master, fly_context_t *ctx, __fly_unused struct inotify_event *ie)
{
	int wd;
	fly_mount_parts_t *parts = NULL;
	struct fly_mount_parts_file *pf = NULL;

	wd = ie->wd;
	/* occurred in mount point directory */
	parts = fly_parts_from_wd(wd, ctx->mount);
	if (parts){
#ifdef DEBUG
		printf("MASTER: Detect at mount point\n");
#endif
		return __fly_inotify_in_mp(master, parts, ie);
	}

	pf = fly_pf_from_mount(wd, ctx->mount);
	if (pf){
#ifdef DEBUG
		printf("MASTER: Detect at mount point file\n");
#endif
		return __fly_inotify_in_pf(master, pf, ie);
	}

	return 0;
}
#elif defined HAVE_KQUEUE
__fly_static int __fly_inotify_handle(fly_master_t *master, fly_context_t *ctx, fly_event_t *__e)
{
	int fd;

	fd = __e->fd;
	fly_mount_parts_t *parts = NULL;
	struct fly_mount_parts_file *pf = NULL;

	/* occurred in mount point directory */
	parts = fly_parts_from_fd(fd, ctx->mount);
	if (parts != NULL){
#ifdef DEBUG
		printf("MASTER: Detect at mount point\n");
#endif
		if (__fly_inotify_in_mp(master, parts, __e) == -1)
			return -1;
	}

	pf = fly_pf_from_mount(fd, ctx->mount);
	if (pf != NULL){
#ifdef DEBUG
		printf("MASTER: Detect at mount point file\n");
#endif
		if (__fly_inotify_in_pf(master, pf, __e, __e->eflag) == -1)
			return -1;
	}

#ifdef DEBUG
	printf("MASTER MOUNT CONTENT DEBUG\n");
	__fly_debug_mnt_content(ctx);
	printf("MASTER: now total monitorable event count %d\n", fly_queue_count(&__e->manager->monitorable));
	printf("MASTER: now total unmonitorable event count %d\n", fly_queue_count(&__e->manager->unmonitorable));
	fly_master_mount_parts_debug(master, __e->manager, __e);
#endif
	return 0;
}
#endif

#ifdef HAVE_INOTIFY
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

	master = (fly_master_t *) fly_event_data_get(e, __p);
#ifdef DEBUG
	assert(master != NULL);
	assert(master->context != NULL);
#endif
	ctx = master->context;
	inofd = e->fd;
	inobuf_size = FLY_NUMBER_OF_INOBUF*(sizeof(struct inotify_event) + NAME_MAX + 1);
	pool = e->manager->pool;
	inobuf = fly_pballoc(pool, inobuf_size);
	while(true){
		num_read = read(inofd, inobuf, inobuf_size);
		if (num_read == -1){
			if (errno == EAGAIN || errno == EWOULDBLOCK)
				break;
			else{
				goto error;
			}
		}

		for (__ptr=inobuf; __ptr < (char *) (inobuf+num_read);){
			__e = (struct inotify_event *) __ptr;
			if (__fly_inotify_handle(master, ctx, __e) == -1){
#ifdef DEBUG
				perror("__fly_inotify_handle");
				printf("MASTER: Inotify handle error.\n");
#endif
				goto error;
			}
			__ptr += sizeof(struct inotify_event) + __e->len;
		}
	}

	/* release buf */
	fly_pbfree(pool, inobuf);
	return 0;
error:
	fly_pbfree(pool, inobuf);
	return -1;
}

#elif defined HAVE_KQUEUE

static int __fly_master_inotify_handler(fly_event_t *e)
{
	fly_master_t *master;
	fly_context_t *ctx;

	//master = (fly_master_t *) e->event_data;
	master = (fly_master_t *) fly_event_data_get(e, __p);
#ifdef DEBUG
	assert(master != NULL);
	assert(master->context != NULL);
#endif
	ctx = master->context;
	return __fly_inotify_handle(master, ctx, e);
}

#endif

static int __fly_master_inotify_end_handler(fly_event_t *__e)
{
	return close(__e->fd);
}


__fly_static int __fly_master_inotify_event(fly_master_t *master, fly_event_manager_t *manager)
{
	fly_event_t *e;
	fly_context_t *ctx = master->context;

	e = fly_event_init(manager);
	if (fly_unlikely_null(e))
		return -1;

	if (!ctx || !ctx->mount)
		return -1;

#ifdef HAVE_INOTIFY
	int inofd;
	inofd = fly_inotify_init();
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
	//e->event_data = (void *) master;
	fly_event_data_set(e, __p, master);
	FLY_EVENT_HANDLER(e, __fly_master_inotify_handler);
	e->expired = false;
	e->available = false;
	e->if_fail_term = true;
	e->fail_close = fly_master_default_fail_close;
	e->end_handler = __fly_master_inotify_end_handler;
	fly_event_inotify(e);

	return fly_event_register(e);
#else
	if (fly_mount_inotify_kevent(
			manager, ctx->mount, (void *) master, \
			__fly_master_inotify_handler, \
			__fly_master_inotify_end_handler
		) == -1)
		return -1;
	return 0;
#endif
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


#ifdef HAVE_INOTIFY
static struct fly_watch_path *__fly_search_wp(fly_master_t *__m, int wd)
#else
static struct fly_watch_path *__fly_search_wp(fly_master_t *__m, int fd)
#endif
{
	struct fly_bllist *__b;
	struct fly_watch_path *__wp;

	fly_for_each_bllist(__b, &__m->reload_filepath){
		__wp = (struct fly_watch_path *) fly_bllist_data(__b, struct fly_watch_path, blelem);
#ifdef HAVE_INOTIFY
		if (__wp->wd == wd)
#else
		if (__wp->fd == fd)
#endif
			return __wp;
	}
	return NULL;
}

/* reload fly server */
#ifdef HAVE_INOTIFY
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
	fly_notice_direct_log(
		__m->context->log,
		"Detected fly application update. Restart fly server.\n"
	);

	fly_master_release(__m);

	/* jump to master process */
#if defined HAVE_SIGLONGJMP && defined HAVE_SIGSETJMP
	siglongjmp(env, FLY_MASTER_RELOAD);
#else
	longjmp(env, FLY_MASTER_RELOAD);
#endif
}

#elif HAVE_KQUEUE

static int __fly_reload(fly_master_t *__m, int fd, int mask)
{
	struct fly_watch_path *__wp;

	__wp = __fly_search_wp(__m, fd);
	if (__wp == NULL)
		return -1;

	switch(mask){
	case NOTE_DELETE:
		break;
	case NOTE_EXTEND:
		break;
	case NOTE_WRITE:
		break;
	case NOTE_RENAME:
		break;
	default:
		FLY_NOT_COME_HERE
	}
	fly_kill_workers(__m->context);
	fly_notice_direct_log(
		__m->context->log,
		"Detected fly application update. Restart fly server.\n"
	);

	fly_master_release(__m);
	/* jump to master process */
#if defined HAVE_SIGLONGJMP && defined HAVE_SIGSETJMP
	siglongjmp(env, FLY_MASTER_RELOAD);
#else
	longjmp(env, FLY_MASTER_RELOAD);
#endif
}
#endif

#ifdef HAVE_INOTIFY
static int __fly_master_reload_filepath_handler(fly_event_t *e)
{
#define FLY_INOTIFY_BUFSIZE(__c)			((__c)*sizeof(struct inotify_event)+NAME_MAX+1)

	fly_master_t *__m;
	char buf[FLY_INOTIFY_BUFSIZE(1)];
	ssize_t numread;
	struct inotify_event *__ie;

	__m = fly_event_data_get(e, __p);
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

#elif defined HAVE_KQUEUE

static int __fly_master_reload_filepath_handler(fly_event_t *e)
{
	fly_master_t *__m;
	int fflags;

	__m = fly_event_data_get(e, __p);
	/* trigger flag of event(ex. NOTE_EXTEND ) */
	fflags = e->eflag;
	return __fly_reload(__m, e->fd, fflags);
}

#endif

static int __fly_master_reload_filepath_end_handler(fly_event_t *__e)
{
	return close(__e->fd);
}

#ifdef HAVE_INOTIFY
static int __fly_master_reload_filepath(fly_master_t *master, fly_event_manager_t *manager)
{
	if (master->reload_pathcount == 0 || !master->detect_reload)
		return 0;

	int fd;
	int wd;
	fly_event_t *e;

	fd = fly_inotify_init();
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
	//e->event_fase = NULL;
	//e->event_state = NULL;
	fly_event_fase_set(e, __p, NULL);
	fly_event_state_set(e, __p, NULL);
	e->expired = false;
	e->available = false;
	//e->event_data = (void *) master;
	fly_event_data_set(e, __p, master);
	e->if_fail_term = true;
	e->fail_close = fly_master_default_fail_close;
	e->end_handler = __fly_master_reload_filepath_end_handler;
	FLY_EVENT_HANDLER(e, __fly_master_reload_filepath_handler);

	fly_time_null(e->timeout);
	fly_event_inotify(e);
	return fly_event_register(e);
}

#elif defined HAVE_KQUEUE

static int __fly_master_reload_filepath(fly_master_t *master, fly_event_manager_t *manager)
{
	if (master->reload_pathcount == 0 || !master->detect_reload)
		return 0;

	int fd;
	struct fly_bllist *__b;
	struct fly_watch_path *__wp;
	fly_event_t *e;

	fly_for_each_bllist(__b, &master->reload_filepath){
		__wp = (struct fly_watch_path *) fly_bllist_data(__b, struct fly_watch_path, blelem);
		fd = open(__wp->path, O_RDONLY);
		if (fd == -1)
			return -1;
		__wp->fd = fd;

		e = fly_event_init(manager);
		if (e == NULL)
			return -1;

		e->fd = fd;
		e->read_or_write = FLY_KQ_RELOAD;
		e->flag = FLY_PERSISTENT;
		e->tflag = FLY_INFINITY;
		e->eflag = (NOTE_DELETE|NOTE_EXTEND|NOTE_WRITE|NOTE_RENAME);
		e->expired = false;
		e->available = false;
		fly_event_data_set(e, __p, master);
		e->if_fail_term = true;
		e->fail_close = fly_master_default_fail_close;
		e->end_handler = __fly_master_reload_filepath_end_handler;
		FLY_EVENT_HANDLER(e, __fly_master_reload_filepath_handler);

		fly_time_null(e->timeout);
		fly_event_reload(e);

		if (fly_event_register(e) == -1)
			return -1;
	}
	return 0;
}

#endif


#ifdef HAVE_INOTIFY
int fly_inotify_init(void)
{
#ifdef HAVE_INOTIFY_INIT1
	return inotify_init1(IN_CLOEXEC|IN_NONBLOCK);
#else
	int ifd, flag;

	ifd = inotify_init();
	if (ifd == -1)
		return -1;

	flag = fcntl(ifd, F_GETFD);
	if (flag == -1)
		return -1;
	if (fcntl(ifd, F_SETFD, flag|FD_CLOEXEC) == -1)
		return -1;

	flag = fcntl(ifd, F_GETFL);
	if (flag == -1)
		return -1;
	if (fcntl(ifd, F_SETFL, flag|O_NONBLOCK) == -1)
		return -1;

	return ifd;
#endif
}
#endif

#if defined DEBUG && defined HAVE_KQUEUE
static void fly_master_mount_parts_debug(fly_master_t *master, fly_event_manager_t *__m, fly_event_t *event)
{
	fly_mount_t *mount;
	fly_mount_parts_t *__p;
	struct fly_mount_parts_file *__pf;
	struct fly_bllist *__b, *__b2;
	struct fly_event *__e;
	struct fly_queue *__q;

	mount = master->context->mount;
	printf("~~~~~~~~~~~~~~~~ MASTER MOUNT DEBUG ~~~~~~~~~~~~~~~~\n");
	printf("event fd: %d\n", event->fd);
	fly_for_each_bllist(__b, &mount->parts){
		__p = fly_bllist_data(__b, fly_mount_parts_t, mbelem);
		printf("\tmount point %s: \n", __p->mount_path);
		fly_for_each_bllist(__b2, &__p->files){
			__pf = fly_bllist_data(__b2, struct fly_mount_parts_file, blelem);
			printf("\t\tpath %s: %s, fd: %d\n", __p->mount_path, __pf->filename, __pf->fd);
			assert(fly_event_from_fd(__pf->event->manager, __pf->fd) != NULL);
		}
	}

	fly_for_each_queue(__q, &__m->monitorable){
		__e = fly_queue_data(__q, struct fly_event, qelem);
		printf("\tevent fd %d\n", __e->fd);
		if (fly_event_is_inotify(__e)){
			printf("\tinotify event\n");
			fly_for_each_bllist(__b, &mount->parts){
				__p = fly_bllist_data(__b, fly_mount_parts_t, mbelem);
				if (fly_pf_from_fd(__e->fd, __p) != NULL ||
						__e->fd == event->fd)
					goto next_event;
				if (__p->fd == __e->fd)
					goto next_event;
			}
			printf("\t!!!! Not found event fd %d !!!!\n", __e->fd);
			assert(0);
next_event:
			continue;
		}
	}
	fly_for_each_queue(__q, &__m->unmonitorable){
		__e = fly_queue_data(__q, struct fly_event, qelem);
		assert(!fly_event_is_inotify(__e));
	}
	printf("~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~\n");
	return;
}
#endif
