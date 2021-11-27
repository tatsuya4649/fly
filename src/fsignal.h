#ifndef _FLY_SIGNAL_H
#define _FLY_SIGNAL_H
#include <signal.h>
#ifdef HAVE_SIGNALFD
#include <sys/signalfd.h>
#endif
#include "util.h"
#include "context.h"
#include "bllist.h"


typedef int32_t fly_signum_t;

struct fly_orig_signal{
	fly_signum_t number;
	struct sigaction		sa;
	struct fly_bllist blelem;
};

#ifdef HAVE_SIGNALFD
typedef struct signalfd_siginfo 	fly_siginfo_t;
#elif defined HAVE_KQUEUE
typedef struct __siginfo			fly_siginfo_t;
#else
#error not found signalfd or kqueue on your system.
#endif

struct fly_signal{
	fly_signum_t number;
	void (*handler)(fly_context_t *ctx, fly_siginfo_t *);
	struct fly_bllist blelem;
};
#define FLY_SIGNAL_SETTING(__sig, __h)	\
				{ .number = __sig, .handler = __h }
typedef void (fly_sighand_t)(fly_context_t *ctx, fly_siginfo_t *);
typedef struct fly_signal fly_signal_t;

void fly_sigint_handler(__fly_unused int signo);
int fly_signal_init(void);

void __fly_only_recv(fly_context_t *ctx, fly_siginfo_t *);
int fly_refresh_signal(void);

extern fly_signum_t fly_signals[];
int fly_signal_register(sigset_t *mask);
__fly_noreturn int fly_signal_default_handler(fly_context_t *, fly_siginfo_t *);
int fly_send_signal(pid_t pid, int signumber, int value);

static inline void FLY_SIG_IGN(fly_context_t *ctx __fly_unused, fly_siginfo_t *__info __fly_unused){}
#endif
