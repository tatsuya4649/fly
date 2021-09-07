#include <assert.h>
#include "context.h"

#define TEST_PORT		1234
int main()
{
	fly_context_t *ctx;
	ctx = fly_context_init();
	assert(ctx != NULL);

	assert(fly_context_release(ctx) != -1);
}
