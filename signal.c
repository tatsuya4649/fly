
#include <stdio.h>
#include <stdlib.h>
#include "signal.h"
#include "fs.h"

int fly_signal_init(void)
{
	struct sigaction intact;

    intact.sa_handler = fly_sigint_handler;
    intact.sa_flags = 0;
    return sigaction(SIGINT, &intact,NULL);
}

void fly_sigint_handler(__unused int signo)
{
    fprintf(stderr, "Interrupt now (Ctrl+C)...");
	fly_fs_release();
    exit(0);
}

