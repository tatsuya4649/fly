#include <stdio.h>
#include <stdlib.h>
#include "fsignal.h"
#include "mount.h"
#include "err.h"

fly_signum_t fly_signals[] = {
	SIGHUP,
	SIGINT,
	SIGQUIT,
	SIGILL,
	SIGTRAP,
	SIGABRT,
	SIGBUS,
	SIGFPE,
	SIGKILL,
	SIGUSR1,
	SIGSEGV,
	SIGUSR2,
	SIGPIPE,
	SIGALRM,
	SIGTERM,
	SIGCHLD,
	SIGCONT,
	SIGSTOP,
	SIGTTIN,
	SIGTTOU,
	SIGURG,
	SIGXCPU,
	SIGXFSZ,
	SIGVTALRM,
	SIGPROF,
	SIGWINCH,
	SIGIO,
#ifdef SIGPWR
	SIGPWR,
#endif
	SIGSYS,
	-1
};

int fly_signal_init(void)
{
	struct sigaction intact;

	sigemptyset(&intact.sa_mask);
    intact.sa_handler = fly_sigint_handler;
    intact.sa_flags = 0;
    if (sigaction(SIGINT, &intact,NULL) == -1){
		return FLY_ERROR(errno);
	}
	return FLY_SUCCESS;
}

__attribute__((noreturn)) void fly_sigint_handler(__fly_unused int signo)
{
    fprintf(stderr, "Interrupt now (Ctrl+C)...\n");
    exit(0);
}


void __fly_only_recv(fly_context_t *ctx __fly_unused, fly_siginfo_t *info __fly_unused)
{
	return;
}

int fly_refresh_signal(void)
{
	for (fly_signum_t *__sig=fly_signals; *__sig>0; __sig++){
#ifdef HAVE_KQUEUE
		if (*__sig!=SIGKILL && *__sig!=SIGSTOP &&  signal(*__sig, SIG_IGN) == SIG_ERR)
#else
		if (*__sig!=SIGKILL && *__sig!=SIGSTOP &&  signal(*__sig, SIG_DFL) == SIG_ERR)
#endif
			return -1;
	}
	return 0;
}

#ifdef HAVE_SIGNALFD
int fly_signal_register(sigset_t *mask)
{
	int sigfd;

	if (sigprocmask(SIG_BLOCK, mask, NULL) == -1)
		return -1;
	sigfd = signalfd(-1, mask, SFD_CLOEXEC|SFD_NONBLOCK);
	return sigfd;
}
#endif

__fly_noreturn int fly_signal_default_handler(fly_context_t *ctx __fly_unused, fly_siginfo_t *info __fly_unused)
{
	exit(0);
}

int fly_send_signal(pid_t pid, int signumber, int value)
{
	return sigqueue(pid, signumber, (const union sigval) value);
}
