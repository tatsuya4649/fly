#ifndef _FLY_SIGNAL_H
#define _FLY_SIGNAL_H
#include <signal.h>
#include <sys/signalfd.h>
#include "util.h"
#include "context.h"

/* detect to modify file in mount points */
#define FLY_SIGNAL_MODF					(SIGRTMIN)
/* detect to add file in mount points */
#define FLY_SIGNAL_ADDF					(SIGRTMIN+1)
/* detect to delete file in mount points */
#define FLY_SIGNAL_DELF					(SIGRTMIN+2)
/* detect to unmount mount point */
#define FLY_SIGNAL_UMOU					(SIGRTMIN+3)

#define FLY_RTSIGSET(s, sset)								\
	do{												\
		if (sigaddset((sset), FLY_SIGNAL_ ## s) == -1)	\
			return -1;								\
		if (__fly_add_worker_sigs(ctx, FLY_SIGNAL_ ## s, FLY_SIGNAL_ ## s ## _HANDLER) == -1)												\
			return -1;								\
	} while (0)

typedef int32_t fly_signum_t;
struct fly_signal{
	fly_signum_t number;
	void (*handler)(fly_context_t *ctx, struct signalfd_siginfo *);
	struct fly_signal *next;
};
typedef void (fly_sighand_t)(fly_context_t *ctx, struct signalfd_siginfo *);
typedef struct fly_signal fly_signal_t;

void fly_sigint_handler(__unused int signo);
int fly_signal_init(void);

void __fly_only_recv(fly_context_t *ctx, struct signalfd_siginfo *);
int fly_refresh_signal(void);

extern fly_signum_t fly_signals[];
int fly_signal_register(sigset_t *mask);
__noreturn int fly_signal_default_handler(struct signalfd_siginfo *);
int fly_send_signal(pid_t pid, int signumber, int value);
#endif
