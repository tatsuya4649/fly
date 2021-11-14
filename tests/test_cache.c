
#include <assert.h>
#include "cache.h"
#include "mount.h"
#include "context.h"

int main()
{
	fly_context_t ctx;
	struct fly_mount m;
	struct fly_mount_parts ps;
	struct fly_mount_parts_file pf;
	char *filename = "HelloWorld";

	ctx.pool = fly_create_pool(100);
	m.ctx = &ctx;
	ps.mount = &m;

	pf.fd = open("test/test_cache.c", O_RDONLY);
	strcpy(pf.filename, filename);
	pf.parts = &ps;
	pf.hash = NULL;
	pf.next = NULL;

	assert(fly_hash_from_parts_file(&pf));
}
