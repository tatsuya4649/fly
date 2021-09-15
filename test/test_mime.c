#include <assert.h>
#include <stdio.h>
#include "mime.h"
#include "request.h"
#include "header.h"

__fly_static int __fly_accept_mime_parse(__unused fly_mime_t *mime, fly_hdr_value *value);
int main()
{
	fly_mime_t mime;
	fly_request_t req;

	req.pool = fly_create_pool(100);
	req.request_line = NULL;
	req.header = NULL;
	req.body = NULL;
	req.buffer = NULL;

	fly_hdr_value *value = "text/plain; charset=utf_8; q=0.5; hello=world, text/html, text/x-dvi; q=0.8, text/x-c";
	printf("%s\n", value);

	mime.acqty = 0;
	mime.accepts = NULL;
	mime.request = &req;
	req.mime = &mime;
	assert(__fly_accept_mime_parse(&mime, value));
	return 0;
}
