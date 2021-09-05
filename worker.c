#include "worker.h"

int fly_worker_init()
{
	int cpid;
	cpid = fork();
	switch (cpid){
	case 0:
		/* child */
		return 0;
	case -1:
		/* error */
		return -1;
	default:
		/* parent */
		return cpid;
	}
}
