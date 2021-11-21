#ifndef _FLY_SIGNAL_H
#define _FLY_SIGNAL_H
#include <signal.h>
#include <sys/signalfd.h>
#include "util.h"
#include "context.h"
#include "bllist.h"


typedef int32_t fly_signum_t;

struct fly_orig_signal{
	fly_signum_t number;
	struct sigaction		sa;
	struct fly_bllist blelem;
};

struct fly_signal{
	fly_signum_t number;
	void (*handler)(fly_context_t *ctx, struct signalfd_siginfo *);
	struct fly_bllist blelem;
};
#define FLY_SIGNAL_SETTING(__sig, __h)	\
				{ .number = __sig, .handler = __h }
typedef void (fly_sighand_t)(fly_context_t *ctx, struct signalfd_siginfo *);
typedef struct fly_signal fly_signal_t;

void fly_sigint_handler(__unused int signo);
int fly_signal_init(void);

void __fly_only_recv(fly_context_t *ctx, struct signalfd_siginfo *);
int fly_refresh_signal(void);

extern fly_signum_t fly_signals[];
int fly_signal_register(sigset_t *mask);
__noreturn int fly_signal_default_handler(fly_context_t *, struct signalfd_siginfo *);
int fly_send_signal(pid_t pid, int signumber, int value);

static inline void FLY_SIG_IGN(fly_context_t *ctx __unused, struct signalfd_siginfo *__info __unused){}
#endif
