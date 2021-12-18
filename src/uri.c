#include <string.h>
#include "uri.h"
#include "mount.h"
#include "request.h"

bool fly_is_uri_index(fly_uri_t *uri)
{
	const char *index_path;
	char *ptr = uri->ptr;
	int i=0;

	while(fly_slash(*ptr++)){
		i++;
		/* only slash */
		if (uri->len <= (size_t) i)
			return true;
	}

	index_path = fly_index_path();
	if (strncmp(index_path, ptr, strlen(index_path)) == 0)
		return true;
	else
		return false;
}

int fly_query_parse_from_uri(struct fly_request *req, fly_uri_t *uri)
{
	struct fly_request_line *reqline;

	reqline = req->request_line;
#ifdef DEBUG
	assert(reqline != NULL);
	assert(uri != NULL);
	assert(uri->ptr != NULL);
#endif
	if (fly_unlikely_null(reqline))
		return -1;

	char *q_ptr, *uri_last_ptr;

	uri_last_ptr = uri->ptr + uri->len - 1;
	q_ptr = memchr(uri->ptr, FLY_QUESTION, uri->len);
	/*
	 *	 No query
	 *		/user/10 or /user/10?
	 */
	if (q_ptr == NULL && (uri_last_ptr == (q_ptr)))
		return 0;

	/*
	 *	 /user/10?user_id=10&username=user
	 *			  ~		 query_len		 ~
	 *			  ---------------------->
	 *		    q_ptr				 uri_last_ptr
	 */
	fly_query_set(req, q_ptr+1, uri_last_ptr-q_ptr);
	return 0;
}
