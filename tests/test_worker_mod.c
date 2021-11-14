#include <assert.h>
#include <stdlib.h>
#include "worker.h"
#include "server.h"
#include "response.h"

#define TEST_PORT			"1234"

int main()
{
	fly_context_t *ctx;
	fly_route_reg_t *reg;

	if (setenv(FLY_PORT_ENV, TEST_PORT, 1) == -1)
		return -1;

	ctx = fly_context_init();
	if (fly_mount_init(ctx) == -1)
		return -1;
	if (fly_mount(ctx, "./mnt") == -1)
		return -1;

	ctx->mount->parts->infd = inotify_init();

	fly_worker_process(ctx, NULL);
}
