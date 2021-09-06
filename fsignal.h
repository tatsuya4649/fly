#ifndef _FLY_SIGNAL_H
#define _FLY_SIGNAL_H
#include <signal.h>
#include "util.h"


typedef int fly_signum_t;
struct fly_signal{
	fly_signum_t number;
	void (*handler)(int, siginfo_t *, void *);
};
typedef struct fly_signal fly_signal_t;

void fly_sigint_handler(__unused int signo);
int fly_signal_init(void);

void __fly_only_recv(int, siginfo_t *, void *);

extern fly_signum_t fly_signals[];
#endif
