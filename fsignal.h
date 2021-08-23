#ifndef _FLY_SIGNAL_H
#define _FLY_SIGNAL_H
#include <signal.h>
#include "util.h"

void fly_sigint_handler(__unused int signo);
int fly_signal_init(void);

#endif
