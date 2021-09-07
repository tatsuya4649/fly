#include <assert.h>
#include <stdlib.h>
#include "worker.h"
#include "server.h"

#define TEST_PORT			"1234"
int main()
{
	fly_context_t *ctx;

	if (setenv(FLY_PORT_ENV, TEST_PORT, 1) == -1)
		return -1;

	ctx = fly_context_init();

	fly_worker_process(ctx, NULL);
}
