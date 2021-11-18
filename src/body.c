#include "body.h"
#include "request.h"
#include "context.h"

fly_body_t *fly_body_init(fly_context_t *ctx)
{
	fly_pool_t *pool;
	fly_body_t *body;

	pool = fly_create_pool(ctx->pool_manager, FLY_REQBODY_SIZE);
	body = fly_pballoc(pool, sizeof(fly_body_t));

	body->pool = pool;
	body->body = NULL;
	body->body_len = 0;
	body->next_ptr = NULL;

	fly_bllist_init(&body->multipart_parts);
	body->multipart_count = 0;
	body->multipart = false;

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
	if (buffer == NULL || buffer->use_len == 0)
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
	if (fly_unlikely_null(de)){
		return NULL;
	}

	size_t __max;
	__max = fly_max_request_length();
	de->decbuf = fly_buffer_init(de->pool, FLY_BODY_DECBUF_INIT_LEN, FLY_BODY_DECBUF_CHAIN_MAX(__max), FLY_BODY_DECBUF_PER_LEN);
	de->decbuflen = FLY_BODY_DECBUF_INIT_LEN;
	if (de->decbuf == NULL){
		return NULL;
	}
	if (!fly_d_buf_add(de))
		return NULL;

	de->already_ptr = body_c->use_ptr;
	de->already_len = content_length;
	de->target_already_alloc = true;

	if(t->decode(de) == -1)
		return NULL;

	body->body = fly_pballoc(body->pool,de->decbuf->use_len);
	if (fly_unlikely_null(body->body)){
		return NULL;
	}
	body->body_len = de->decbuf->use_len;

	fly_buffer_memcpy_all(body->body, de->decbuf);
	/* release resource */
	fly_de_release(de);
	return body->body;
}

static int __fly_multipart_parse_line(struct fly_body_parts_header *__ph, char **ptr, char *last_ptr)
{
	enum{
		INIT,
		NAME,
		NAME_SPACE,
		COLON,
		COLON_SPACE,
		VALUE,
		ADD,
		END,
	} status;

	char *name_ptr, *value_ptr;
	size_t name_len, value_len, prefix_len;

/* RFC7578: 4.8 */
#define FLY_BODY_MIME_HEADER_PREFIX			"Content-"
	prefix_len = strlen(FLY_BODY_MIME_HEADER_PREFIX);
	status = INIT;
	while((*ptr) < last_ptr){
		switch(status){
		case INIT:
			if (strncmp(*ptr, FLY_BODY_MIME_HEADER_PREFIX, prefix_len) != 0)
				/* parse_error */
				return -1;

			name_ptr = *ptr;
			status = NAME;
			continue;
		case NAME:
			if (fly_space(**ptr) || fly_ht(**ptr)){
				name_len = *ptr - name_ptr;
				status = NAME_SPACE;
				break;
			}
			if (fly_colon(**ptr)){
				name_len = *ptr - name_ptr;
				status = COLON;
				break;
			}
			break;
		case NAME_SPACE:
			if (fly_space(**ptr) || fly_ht(**ptr))
				break;

			if (fly_colon(**ptr)){
				status = COLON;
				break;
			}

			goto error;
		case COLON:
			if (fly_space(**ptr) || fly_ht(**ptr)){
				status = COLON_SPACE;
				continue;
			}

			value_ptr = *ptr;
			status = VALUE;
			continue;
		case COLON_SPACE:
			if (fly_space(**ptr) || fly_ht(**ptr))
				break;

			value_ptr = *ptr;
			status = VALUE;
			continue;
		case VALUE:
			if (fly_cr((*ptr)[0]) && fly_lf((*ptr)[1])){
				value_len = *ptr - value_ptr;
				status = ADD;
			}
			break;
		case ADD:
			__ph->name = name_ptr;
			__ph->name_len = name_len;
			__ph->value = value_ptr;
			__ph->value_len = value_len;

			status = END;
			continue;
		case END:
			return 0;
		default:
			FLY_NOT_COME_HERE
		}

		(*ptr)++;
		if (*ptr >= last_ptr && status != END)
			goto error;
	}
	return 0;
error:
	return -1;
}

static struct fly_body_parts_header *__fly_body_parts_header_init(struct fly_body *body)
{
		struct fly_body_parts_header *__bph;
		__bph = fly_pballoc(body->pool, sizeof(struct fly_body_parts_header));
		if (fly_unlikely_null(__bph))
			return NULL;

		__bph->name = NULL;
		__bph->value = NULL;
		__bph->name_len = 0;
		__bph->value_len = 0;
		return __bph;
}

static struct fly_body_parts *__fly_body_parts_init(struct fly_body *body)
{
	struct fly_body_parts *__pb;

	__pb = fly_pballoc(body->pool, sizeof(struct fly_body_parts));
	if (fly_unlikely_null(__pb))
		return NULL;

	__pb->header_count = 0;
	__pb->parts_len = 0;
	__pb->ptr = NULL;
	fly_bllist_init(&__pb->headers);

	return __pb;
}

static int __fly_multipart_header_parse(char **ptr, struct fly_body_parts *__p, fly_body_t *body)
{
	while(((*ptr) < (body->body + body->body_len - 1)) && !(fly_cr((*ptr)[0]) && fly_lf((*ptr)[1]))){
		struct fly_body_parts_header *__bph;

		__bph = __fly_body_parts_header_init(body);
		if (fly_unlikely_null(__bph))
			return -1;

		/* parse header */
		if (__fly_multipart_parse_line(__bph, ptr, body->body + body->body_len - 1) == -1)
			return -1;

		fly_bllist_add_tail(&__p->headers, &__bph->blelem);
		__p->header_count++;
		(*ptr)++;
	}

	(*ptr) += FLY_CRLF_LENGTH;
	return 0;
}

void fly_body_parse_multipart(fly_request_t *req)
{
	fly_body_t *body;
	fly_hdr_ci *ci;
	fly_hdr_c *__c;
	__unused char *boundary;
	struct fly_bllist *__b;

	body = req->body;
	ci = req->header;
#ifdef DEBUG
	assert(body != NULL);
	assert(ci != NULL);
#endif
	if (body->body_len == 0 && ci->chain_count == 0)
		return;

	fly_for_each_bllist(__b, &ci->chain){
		__c = fly_bllist_data(__b, fly_hdr_c, blelem);
		if (__c->name_len>0 && \
				(strcmp(__c->name, "Content-Type") == 0 || \
				strcmp(__c->name, "content-type") == 0) && \
				__c->value)
			goto body_parse;
	}
	return;

body_parse:
	;

	char *__p=__c->value;
	char *body_ptr;
	__unused size_t boundary_len;
	while(!fly_semicolon(*__p)){
		__p++;
		if (__p >= (__c->value+__c->value_len-1))
			return;
	}
	while(!fly_space(*__p)){
		__p++;
		if (__p >= (__c->value+__c->value_len-1))
			return;
	}
	while(!fly_equal(*__p)){
		__p++;
		if (__p >= (__c->value+__c->value_len-1))
			return;
	}
	__p++;
	/* start of boundary */
	boundary = __p;
	boundary_len = __c->value+__c->value_len-__p;
	__p = boundary;
	body_ptr = body->body;

#define FLY_BOUNDARY_START_LENGTH			2
#define FLY_BOUNDARY_START					"--"
	while(body_ptr <= (body->body+body->body_len-1)){
		if (!fly_minus(body_ptr[0]) || !fly_minus(body_ptr[1])){
			/* not boundary */
			body_ptr++;
			continue;
		}
		body_ptr += FLY_BOUNDARY_START_LENGTH;

		size_t i=0;
		while(boundary[i++] == *body_ptr++){
			if (i >= boundary_len){
				struct fly_body_parts *__pb;

				if (fly_minus(body_ptr[0]) && fly_minus(body_ptr[1])){
					/* end of parse */
					/* previous content_length setting */
					if (body->multipart_count > 0){
						__pb = (struct fly_body_parts *) \
							   fly_bllist_data(body->multipart_parts.prev, struct fly_body_parts, blelem);
						__pb->parts_len = (body_ptr-i)-__pb->ptr-(2*FLY_CRLF_LENGTH);
					}
					return;
				}else if (!fly_cr(body_ptr[0]) || !fly_lf(body_ptr[1]))
					/* parse error */
					return;

				if (&body_ptr[1] >= (body->body+body->body_len-1))
					/* end of parse */
					return;
				body_ptr += FLY_CRLF_LENGTH;

				__pb = __fly_body_parts_init(body);
				if (fly_unlikely_null(__pb))
					return;

				/* parse body parts header*/
				if (__fly_multipart_header_parse(&body_ptr, __pb, body) == -1)
					/* error */
					return;

				__pb->ptr = body_ptr;
				fly_bllist_add_tail(&body->multipart_parts, &__pb->blelem);
				/* previous content_length setting */
				if (body->multipart_count > 0){
					__pb = (struct fly_body_parts *) \
						   fly_bllist_data(body->multipart_parts.prev, struct fly_body_parts, blelem);
					__pb->parts_len = (body_ptr-i)-__pb->ptr-(FLY_CRLF_LENGTH);
				}
				body->multipart_count++;
				body->multipart = true;
			}
		}
	}
}

struct fly_body_parts *fly_body_add_multipart_parts(struct fly_body *__b)
{
	struct fly_body_parts *__p;

	__p = fly_pballoc(__b->pool, sizeof(struct fly_body_parts));
	if (fly_unlikely_null(__p))
		return NULL;

	return __p;
}
