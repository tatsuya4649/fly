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

int fly_body_release(fly_body_t *body)
{
	return fly_delete_pool(&body->pool);
}

int fly_body_setting(fly_body_t *body, fly_bodyc_t *buffer, size_t content_length)
{
	if (body == NULL)
		return -1;
	body->body = buffer;
	body->body_len = content_length;
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

fly_bodyc_t *fly_decode_body(fly_bodyc_t *bptr, fly_encoding_type_t *t, fly_body_t *body, size_t content_length)
{
	struct fly_de *de;
	size_t decoded_bodylen;

	de = fly_de_init(body->pool);
	if (fly_unlikely_null(de))
		return NULL;

	if (!fly_e_buf_add(de))
		return NULL;
	if (!fly_d_buf_add(de))
		return NULL;

	de->already_ptr = bptr;
	de->already_len = content_length;
	de->target_already_alloc = true;

	if(t->decode(de) == -1)
		return NULL;

	/* decode->body */
	decoded_bodylen = 0;
	for (int i=0; i<de->decbuflen; i++)
		decoded_bodylen += de->decbuf[i].uselen;

	body->body = fly_pballoc(body->pool, sizeof(fly_bodyc_t)*(decoded_bodylen));
	if (fly_unlikely_null(body->body))
		return NULL;
	body->body_len = decoded_bodylen;

	/* copy decoded content */
	decoded_bodylen = 0;
	for (int i=0; i<de->decbuflen; i++){
		memcpy(
			body->body+decoded_bodylen,
			de->decbuf[i].buf,
			de->decbuf[i].uselen
		);
		decoded_bodylen += de->decbuf[i].uselen;
	}
	/* release resource */
	fly_de_release(de);
	return body->body;
}
