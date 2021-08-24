#include "body.h"

fly_body_t *fly_body_init(void)
{
	fly_pool_t *pool;
	fly_body_t *body;

	pool = fly_create_pool(FLY_REQBODY_SIZE);
	if (pool == NULL)
		return NULL;

	body = fly_pballoc(pool, sizeof(fly_body_t));
	
	body->pool = pool;
	body->body = NULL;
	body->body_len = 0;

	return body;
}

int fly_body_release(fly_pool_t *pool)
{
	return fly_delete_pool(pool);
}

int fly_body_setting(fly_body_t *body, fly_bodyc_t *buffer)
{
	if (body == NULL || buffer == NULL)
		return -1;
	body->body = buffer;
	body->body_len = buffer ? strlen(buffer) : 0;
	return 0;
}

fly_bodyc_t *fly_get_body_ptr(char *buffer)
{
    char *newline_point;

	if (buffer == NULL)
		return NULL;
    newline_point = strstr(buffer, "\r\n\r\n");
    if (newline_point != NULL)
        return newline_point + 2*FLY_CRLF_LENGTH;
    return NULL;
}
