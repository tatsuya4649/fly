#include <assert.h>
#include "lang.h"
#include "header.h"
#include "request.h"

__fly_static int __fly_al_parse(fly_lang_t *cs, fly_hdr_value *accept_lang);

int main()
{
	fly_request_t req;
	fly_lang_t l;

	req.pool = fly_create_pool(100);
	l.langqty = 0;
	l.request = &req;

	char *value = "da, en-gb;q=0.8, en;q=0.7";

	assert(__fly_al_parse(&l, value));
}
