#include "body.h"
#include "request.h"
#include "context.h"

fly_body_t *fly_body_init(fly_context_t *ctx)
{
	fly_pool_t *pool;
	fly_body_t *body;

	pool = fly_create_pool(ctx->pool_manager, FLY_REQBODY_SIZE);
	if (pool == NULL)
		return NULL;

	body = fly_pballoc(pool, sizeof(fly_body_t));

	body->pool = pool;
	body->body = NULL;
	body->body_len = 0;

	return body;
}

void fly_body_release(fly_body_t *body)
{
	fly_delete_pool(body->pool);
}

void fly_body_setting(fly_body_t *body, char *ptr, size_t content_length)
{
#ifdef DEBUG
	assert(body && ptr);
#endif
	body->body = ptr;
	body->body_len = content_length;
}

fly_buffer_c *fly_get_body_buf(fly_buffer_t *buffer)
{
	if (buffer == NULL)
		return NULL;

	return fly_buffer_first_chain(buffer);
}

fly_bodyc_t *fly_decode_nowbody(fly_request_t *request, fly_encoding_type_t *t)
{
	struct fly_de *de;
	fly_bodyc_t *nowbody;

	de = fly_de_init(request->body->pool);
	if (fly_unlikely_null(de))
		return NULL;

	size_t __max;
	__max = fly_max_request_length();
	de->decbuf = fly_buffer_init(de->pool, FLY_BODY_DECBUF_INIT_LEN, FLY_BODY_DECBUF_CHAIN_MAX(__max), FLY_BODY_DECBUF_PER_LEN);
	de->decbuflen = FLY_BODY_DECBUF_INIT_LEN;

	nowbody = request->body->body;
	de->type = FLY_DE_DECODE;
	de->already_ptr = request->body->body;
	de->already_len = request->body->body_len;
	de->target_already_alloc = true;

	if(t->decode(de) == -1)
		return NULL;

	/* decode->body */
	request->body->body = fly_pballoc(request->body->pool, de->decbuf->use_len);
	if (fly_unlikely_null(request->body->body))
		return NULL;
	request->body->body_len = de->decbuf->use_len;

	/* copy decoded content */
	fly_buffer_memcpy(request->body->body, fly_buffer_first_ptr(de->decbuf), fly_buffer_first_chain(de->decbuf),  de->decbuf->use_len);
	/* release resource */
	fly_de_release(de);
	fly_pbfree(request->body->pool, nowbody);
	return request->body->body;
}

fly_bodyc_t *fly_decode_body(fly_buffer_c *body_c, fly_encoding_type_t *t, fly_body_t *body, size_t content_length)
{
	struct fly_de *de;

	de = fly_de_init(body->pool);
	if (fly_unlikely_null(de))
		return NULL;

	size_t __max;
	__max = fly_max_request_length();
	de->decbuf = fly_buffer_init(de->pool, FLY_BODY_DECBUF_INIT_LEN, FLY_BODY_DECBUF_CHAIN_MAX(__max), FLY_BODY_DECBUF_PER_LEN);
	de->decbuflen = FLY_BODY_DECBUF_INIT_LEN;
	if (!fly_e_buf_add(de))
		return NULL;
	if (!fly_d_buf_add(de))
		return NULL;

	de->already_ptr = body_c->use_ptr;
	de->already_len = content_length;
	de->target_already_alloc = true;

	if(t->decode(de) == -1)
		return NULL;

	body->body = fly_pballoc(body->pool,de->decbuf->use_len);
	if (fly_unlikely_null(body->body))
		return NULL;
	body->body_len = de->decbuf->use_len;

	fly_buffer_memcpy_all(body->body, de->decbuf);
	/* release resource */
	fly_de_release(de);
	return body->body;
}
