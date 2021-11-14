
#include "context.h"
#include "mount.h"

#define TEST_PORT			"1234"
int main()
{
	fly_context_t *ctx;

	if (setenv(FLY_PORT_ENV, TEST_PORT, 1) == -1)
		return -1;
	ctx = fly_context_init();
	if (ctx == NULL)
		return 1;

	if (fly_mount_init(ctx) == -1)
		return 1;

	if (fly_mount(ctx->mount, ".") == -1)
		return 1;
}

