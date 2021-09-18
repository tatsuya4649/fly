#ifndef _FLY_SIGNAL_H
#define _FLY_SIGNAL_H
#include <signal.h>
#include <sys/signalfd.h>
#include "util.h"
#include "context.h"


typedef int32_t fly_signum_t;
struct fly_signal{
	fly_signum_t number;
	void (*handler)(fly_context_t *ctx, struct signalfd_siginfo *);
};
typedef struct fly_signal fly_signal_t;

void fly_sigint_handler(__unused int signo);
int fly_signal_init(void);

void __fly_only_recv(fly_context_t *ctx, struct signalfd_siginfo *);
int fly_refresh_signal(void);

extern fly_signum_t fly_signals[];
int fly_signal_register(sigset_t *mask);
__noreturn int fly_signal_default_handler(struct signalfd_siginfo *);
#endif
