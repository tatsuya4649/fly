#include <assert.h>
#include "charset.h"
#include "request.h"
#include "alloc.h"

__fly_static int __fly_ac_parse(fly_charset_t *cs, fly_hdr_value *accept_charset);
int main()
{
	fly_request_t req;
	fly_charset_t cs;

	req.pool = fly_create_pool(100);
	cs.charqty = 0;
	cs.request = &req;

	char *value = "iso-8859-5, unicode-1-1;q=0.8";

	assert(__fly_ac_parse(&cs, value));
}
