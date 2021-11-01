#include <stdio.h>
#include <time.h>
#include <string.h>
#include <stdlib.h>
#include "header.h"
#include "body.h"
#include "mime.h"
#include "math.h"
#include "cache.h"
#include "request.h"
#include "uri.h"
#include "v2.h"
#include "response.h"

fly_hdr_ci *fly_header_init(fly_context_t *ctx)
{
	fly_pool_t *pool = fly_create_pool(ctx->pool_manager, FLY_HEADER_POOL_PAGESIZE);
	if (pool == NULL)
		return NULL;

	fly_hdr_ci *chain_info;
	chain_info = fly_pballoc(pool, sizeof(fly_hdr_ci));
	chain_info->pool = pool;
	fly_bllist_init(&chain_info->chain);
	chain_info->chain_count = 0;
	return chain_info;
}

void fly_header_release(fly_hdr_ci *info)
{
	fly_delete_pool(info->pool);
}

void __fly_header_add_ci(fly_hdr_c *c, fly_hdr_ci *ci, bool beginning)
{
	if (beginning)
		fly_bllist_add_head(&ci->chain, &c->blelem);
	else
		fly_bllist_add_tail(&ci->chain, &c->blelem);
	ci->chain_count++;
}
/*
 * WARNING: name_len and value_len is length not including end of '\0'.
 */
fly_hdr_c *__fly_header_chain_init(fly_hdr_ci *ci)
{
	fly_hdr_c *c;

	c = fly_pballoc(ci->pool, sizeof(fly_hdr_c));
	if (fly_unlikely_null(c))
		return NULL;

	c->name = NULL;
	c->value = NULL;
	c->name_len = 0;
	c->value_len = 0;
	c->index = 0;
	c->hname_len = 0;
	c->hvalue_len = 0;
	c->hen_name = NULL;
	c->hen_value = NULL;
	c->index_update = 0;
	c->name_index = false;
	c->static_table = false;
	c->dynamic_table = false;
	c->huffman_name = false;
	c->huffman_value = false;
	return c;
}

fly_hdr_c *__fly_header_add(fly_hdr_ci *chain_info, fly_hdr_name *name, size_t name_len, fly_hdr_value *value, size_t value_len, bool beginning)
{
	fly_hdr_c *new_chain;

	new_chain = __fly_header_chain_init(chain_info);
	if (fly_unlikely_null(new_chain))
		return NULL;

	if (name_len){
		new_chain->name = fly_pballoc(chain_info->pool, sizeof(fly_hdr_name)*(name_len+1));
		if (fly_unlikely_null(new_chain->name))
			return NULL;
		new_chain->name_len = name_len;
		memset(new_chain->name, '\0', name_len+1);
		memcpy(new_chain->name, name, name_len);
	}

	if (value_len){
		new_chain->value = fly_pballoc(chain_info->pool, sizeof(fly_hdr_value)*(value_len+1));
		if (fly_unlikely_null(new_chain->value))
			return NULL;
		new_chain->value_len = value_len;
		memset(new_chain->value, '\0', value_len+1);
		memcpy(new_chain->value, value, value_len);
	}

	__fly_header_add_ci(new_chain, chain_info, beginning);
	return new_chain;
}

int fly_header_add_ifno(fly_hdr_ci *chain_info, fly_hdr_name *name, size_t name_len, fly_hdr_value *value, size_t value_len)
{
	fly_hdr_c *__c;

	struct fly_bllist *__b;
	fly_for_each_bllist(__b, &chain_info->chain){
		__c = fly_bllist_data(__b, fly_hdr_c, blelem);
		if (strcmp(__c->name, name) == 0)
			return 0;
	}

	if (__fly_header_add(chain_info, name, name_len, value, value_len, false))
		return 0;
	else
		return -1;
}

int fly_header_add(fly_hdr_ci *chain_info, fly_hdr_name *name, size_t name_len, fly_hdr_value *value, size_t value_len)
{
	if (__fly_header_add(chain_info, name, name_len, value, value_len, false))
		return 0;
	else
		return -1;
}

/* if yet not have name of header, add */
int fly_header_add_ver_ifno(fly_hdr_ci *ci, fly_hdr_name *name, size_t name_len, fly_hdr_value *value, size_t value_len, bool hv2)
{
	fly_hdr_c *__c;

	struct fly_bllist *__b;
	fly_for_each_bllist(__b, &ci->chain){
		__c = fly_bllist_data(__b, fly_hdr_c, blelem);
		if (strcmp(__c->name, name) == 0)
			return 0;
	}

	if (hv2)
		return fly_header_add_v2(ci, name, name_len, value, value_len, false);
	else
		return fly_header_add(ci, name, name_len, value, value_len);
}

int fly_header_add_ver(fly_hdr_ci *ci, fly_hdr_name *name, size_t name_len, fly_hdr_value *value, size_t value_len, bool hv2)
{
	if (hv2)
		return fly_header_add_v2(ci, name, name_len, value, value_len, false);
	else
		return fly_header_add(ci, name, name_len, value, value_len);
}

fly_hdr_c *fly_header_addc(fly_hdr_ci *chain_info, fly_hdr_name *name, int name_len, fly_hdr_value *value, int value_len, bool beginning)
{
	return __fly_header_add(chain_info, name, name_len, value, value_len, beginning);
}

int fly_header_addb(fly_buffer_c *bc, fly_hdr_ci *chain_info, fly_hdr_name *name, int name_len, fly_hdr_value *value, int value_len)
{
	fly_hdr_c *new_chain;
	new_chain = __fly_header_chain_init(chain_info);
	if (fly_unlikely_null(new_chain))
		return -1;

	new_chain->name = fly_pballoc(chain_info->pool, name_len+1);
	if (fly_unlikely_null(new_chain->name))
		return -1;
	new_chain->name_len = name_len;
	memset(new_chain->name, '\0', name_len+1);
	fly_buffer_memcpy(new_chain->name, name, bc, name_len);

	new_chain->value = fly_pballoc(chain_info->pool, value_len+1);
	if (fly_unlikely_null(new_chain->value))
		return -1;
	new_chain->value_len = value_len;
	memset(new_chain->value, '\0', value_len+1);
	fly_buffer_memcpy(new_chain->value, value, bc, value_len);
	new_chain->name[name_len] = '\0';
	new_chain->value[value_len] = '\0';

	__fly_header_add_ci(new_chain, chain_info, false);
	return 0;
}

int fly_header_addbv(fly_buffer_c *bc, fly_hdr_ci *chain_info, fly_hdr_name *name, int name_len, fly_hdr_value *value, int value_len)
{
	fly_hdr_c *new_chain;
	new_chain = __fly_header_chain_init(chain_info);
	if (fly_unlikely_null(new_chain))
		return -1;

	new_chain->name = fly_pballoc(chain_info->pool, name_len+1);
	if (fly_unlikely_null(new_chain->name))
		return -1;
	new_chain->name_len = name_len;
	memset(new_chain->name, '\0', name_len+1);
	memcpy(new_chain->name, name, name_len);

	new_chain->value = fly_pballoc(chain_info->pool, value_len+1);
	if (fly_unlikely_null(new_chain->value))
		return -1;
	new_chain->value_len = value_len;
	memset(new_chain->value, '\0', value_len+1);
	fly_buffer_memcpy(new_chain->value, value, bc, value_len);

	new_chain->name[name_len] = '\0';
	new_chain->value[value_len] = '\0';

	__fly_header_add_ci(new_chain, chain_info, false);
	return 0;
}

int fly_header_addmodify(fly_hdr_ci *chain_info, fly_hdr_name *name, int name_len, fly_hdr_value *value, int value_len, bool hv2)
{
	fly_hdr_c *__c;

	struct fly_bllist *__b;
	fly_for_each_bllist(__b, &chain_info->chain){
		__c = fly_bllist_data(__b, fly_hdr_c, blelem);
		if (strcmp(__c->name, name) == 0){
			/* release */
			fly_pbfree(chain_info->pool, __c->value);

			__c->value = fly_pballoc(chain_info->pool, value_len+1);
			__c->value_len = value_len;
			__c->value[value_len] = '\0';
			memcpy(__c->value, value, value_len);
			return 0;
		}
	}

	/* if no */
	if (hv2)
		return fly_header_add_v2(chain_info, name, name_len, value, value_len, false);
	else
		return fly_header_add(chain_info, name, name_len, value, value_len);
}

int fly_header_delete(fly_hdr_ci *chain_info, char *name)
{
	struct fly_bllist *__b;
	fly_hdr_c *__c;
	fly_for_each_bllist(__b, &chain_info->chain){
		__c = fly_bllist_data(__b, fly_hdr_c, blelem);
		if (strcmp(__c->name, name) == 0){
			fly_bllist_remove(__b);
			chain_info->chain_count--;
			return 0;
		}
	}

	/* not found */
#ifdef DEBUG
	FLY_NOT_COME_HERE
#endif
	return -1;
}

int fly_strcpy(char *dist, char *src, char *end)
{
	char *d, *s;
	d = dist;
	s = src;
	while (*s){
		*d++ = *s++;
		if (d >= end)
			return -1; }
	return 0;
}

char *fly_chain_string(char *buffer, fly_hdr_c *chain, char *ebuffer)
{
	char *ptr = buffer;
	if (ptr == NULL)
		return NULL;
	if (fly_strcpy(ptr, chain->name, ebuffer) == -1)
		return NULL;
	ptr += strlen(chain->name);
	if (fly_strcpy(ptr, fly_name_hdr_gap(), ebuffer) == -1)
		return NULL;
	ptr += strlen(fly_name_hdr_gap());
	if (fly_strcpy(ptr, chain->value, ebuffer) == -1)
		return NULL;
	ptr += strlen(chain->value);
	if (fly_strcpy(ptr, FLY_CRLF, ebuffer) == -1)
		return NULL;
	ptr += FLY_CRLF_LENGTH;
	/* next header point */
	return ptr;
}

//char *fly_header_from_chain(fly_hdr_ci *chain_info)
//{
//#ifdef DEBUG
//	assert(chain_info != NULL);
//#endif
//
//	char *chain_str;
//	char *ptr;
//	struct fly_bllist *__b;
//	fly_hdr_c *c;
//
//	chain_str= fly_palloc(chain_info->pool, FLY_HEADER_POOL_PAGESIZE);
//	if (fly_unlikely_null(chain_str))
//		return NULL;
//
//	ptr = chain_str;
//	fly_for_each_bllist(__b, &chain_info->chain){
//		c = fly_bllist_data(__b, fly_hdr_c, blelem);
//		ptr = fly_chain_string(ptr, c, chain_str+ (int) fly_byte_convert(FLY_HEADER_POOL_PAGESIZE));
//		if (ptr == NULL)
//			return NULL;
//	}
//	*ptr = '\0';
//	return chain_str;
//}

//size_t fly_hdrlen_from_chain(fly_hdr_ci *chain_info)
//{
//	char *chain_str;
//	chain_str = fly_header_from_chain(chain_info);
//	if (chain_str == NULL)
//		return 0;
//
//	return strlen(chain_str);
//}

fly_buffer_c *fly_get_header_lines_buf(fly_buffer_t *__buf)
{
	fly_buffer_c *__c;
//	char *header;

	__c = fly_buffer_first_chain(__buf);
	return __c;
//	header = fly_buffer_strstr_after(__c, FLY_CRLF);
//	if (header == NULL)
//		return NULL;
//	return *header != '\0' ? header : NULL;
}

long long fly_content_length(fly_hdr_ci *ci)
{
	if (ci->chain_count == 0)
		return 0;

	struct fly_bllist *__b;
	fly_hdr_c *c;

	fly_for_each_bllist(__b, &ci->chain){
		c = fly_bllist_data(__b, fly_hdr_c, blelem);
		if (c->name_len>0 && (strcmp(c->name, "Content-Length") == 0 || strcmp(c->name, "content-length") == 0) && c->value){
			return c->value != NULL ? atoll(c->value) : 0;
		}
	}
	return 0;
}

int fly_connection(fly_hdr_ci *ci)
{
	if (ci->chain_count == 0)
		return FLY_CONNECTION_KEEP_ALIVE;

	struct fly_bllist *__b;
	fly_hdr_c *c;

	fly_for_each_bllist(__b, &ci->chain){
		c = fly_bllist_data(__b, fly_hdr_c, blelem);
		if (strcmp(c->name, "Connection") == 0)
			goto parse_connection;
	}
	return FLY_CONNECTION_KEEP_ALIVE;

parse_connection:
	if (c->value == NULL)
		return FLY_CONNECTION_KEEP_ALIVE;

	if (strcmp(c->value, "close") == 0){
		return FLY_CONNECTION_CLOSE;
	}else if (strcmp(c->value, "keep-alive") == 0){
		return FLY_CONNECTION_KEEP_ALIVE;
	}

	return FLY_CONNECTION_CLOSE;
}

fly_hdr_value *__fly_content_encoding(fly_hdr_ci *ci, const char *conen)
{
	if (ci->chain_count == 0)
		return NULL;

	struct fly_bllist *__b;
	fly_hdr_c *__c;

	fly_for_each_bllist(__b, &ci->chain){
		__c = fly_bllist_data(__b, fly_hdr_c, blelem);
		if (strcmp(__c->name, conen)== 0){
			if (!__c->value)
				return NULL;

			return __c->value;
		}
	}

	return NULL;
}

fly_hdr_value *fly_content_encoding(fly_hdr_ci *ci)
{
	return __fly_content_encoding(ci, "Content-Encoding");
}

fly_hdr_value *fly_content_encoding_s(fly_hdr_ci *ci)
{
	return __fly_content_encoding(ci, "content-encoding");
}

/*
 *		Builtin Header Function
 *
 */
#include "ftime.h"
int fly_add_date(fly_hdr_ci *ci, bool v2)
{
	time_t now;
	char value_field[FLY_DATE_LENGTH];

	now = time(NULL);
	if (now == -1)
		return -1;
	if (fly_imt_fixdate(value_field, FLY_DATE_LENGTH, &now))
		return -1;

	if (v2)
		return fly_header_add_ver_ifno(ci, fly_header_name_length("date"), fly_header_value_length(value_field), v2);
	else
		return fly_header_add_ifno(ci, fly_header_name_length("Date"), fly_header_value_length(value_field));
}

int fly_add_content_type(fly_hdr_ci *ci, fly_mime_type_t *type, bool v2)
{
	if (fly_unlikely_null(type) || \
			fly_unlikely_null(type->name) || fly_mime_invalid(type))
		type = &noext_mime;

	if (v2)
		return fly_header_add_ver_ifno(ci, fly_header_name_length("content-type"), fly_header_value_length(type->name), false);
	else
		return fly_header_add_ifno(ci, fly_header_name_length("Content-Type"), fly_header_value_length(type->name));
}

int fly_add_content_length_from_stat(fly_hdr_ci *ci, struct stat *sb, bool v2)
{
	char *contlen_str;
	int len;

	len = fly_number_ldigits(sb->st_size)+1;
	contlen_str = fly_pballoc(ci->pool, (sizeof(char)*len));
	if (fly_unlikely_null(contlen_str))
		return -1;

	if (snprintf(contlen_str, len, "%ld", (long) sb->st_size) == -1)
		return -1;
	if (v2)
		return fly_header_add_v2(ci, fly_header_name_length("content-length"), fly_header_value_length(contlen_str), false);
	else
		return fly_header_add(ci, fly_header_name_length("Content-Length"), fly_header_value_length(contlen_str));
}

int fly_add_content_length_from_fd(fly_hdr_ci *ci, int fd, bool v2)
{
	struct stat sb;

	if (fstat(fd, &sb) == 1)
		return -1;

	return fly_add_content_length_from_stat(ci, &sb, v2);
}

int fly_add_content_etag(fly_hdr_ci *ci, struct fly_mount_parts_file *pf, bool v2)
{
	if (v2)
		return fly_header_add_v2(ci, fly_header_name_length("etag"), (char *) pf->hash->md5, 2*FLY_MD5_LENGTH, false);
	else
		return fly_header_add(ci, fly_header_name_length("ETag"), (char *) pf->hash->md5, 2*FLY_MD5_LENGTH);
}

int fly_add_last_modified(fly_hdr_ci *ci, struct fly_mount_parts_file *pf, bool v2)
{
	if (v2)
		return fly_header_add_v2(ci, fly_header_name_length("last-modified"), fly_header_value_length((char *) pf->last_modified), false);
	else
		return fly_header_add(ci, fly_header_name_length("Last-Modified"), fly_header_value_length((char *) pf->last_modified));
}

int fly_add_connection(fly_hdr_ci *ci, enum fly_header_connection_e connection)
{
	switch(connection){
	case KEEP_ALIVE:
		return fly_header_add(ci, fly_header_name_length("Connection"), fly_header_value_length("keep-alive"));
	case CLOSE:
		return fly_header_add(ci, fly_header_name_length("Connection"), fly_header_value_length("close"));
	default:
		return -1;
	}
	return -1;
}

int fly_add_content_encoding(fly_hdr_ci *ci, struct fly_encoding_type *e, bool hv2)
{
	char *encname;

#ifdef DEBUG
	assert(e);
#endif

	encname = fly_encname_from_type(e->type);
	if (hv2)
		return fly_header_add_v2(ci, fly_header_name_length("content-encoding"), fly_header_value_length(encname), false);
	else
		return fly_header_add(ci, fly_header_name_length("Content-Encoding"), fly_header_value_length(encname));
}

int fly_add_content_length(fly_hdr_ci *ci, size_t cl, bool hv2)
{
	int len;
	char *contlen_str;

	len = fly_number_ldigits(cl)+1;
	contlen_str = fly_pballoc(ci->pool, (sizeof(char)*len));
	if (fly_unlikely_null(contlen_str))
		return -1;

	if (snprintf(contlen_str, len, "%ld", (long) cl) == -1)
		return -1;

	if (hv2)
		return fly_header_addmodify(ci, fly_header_name_length("content-length"), fly_header_value_length(contlen_str), hv2);
	else
		return fly_header_addmodify(ci, fly_header_name_length("Content-Length"), fly_header_value_length(contlen_str), hv2);
}

int fly_add_allow(fly_hdr_ci *ci, fly_request_t *req)
{
	fly_uri_t *uri;
	fly_route_reg_t *route_reg;
	struct fly_http_method_chain *__c;
	struct fly_http_method *c;
	size_t vallen;
	fly_hdr_value *value, *ptr;

	uri = &req->request_line->uri;
	route_reg = req->ctx->route_reg;

	/* found valid method from registered route */
	__c = fly_valid_method(ci->pool, route_reg, uri->ptr);
	if (fly_unlikely_null(__c))
		return -1;
	/* method->method str */
	vallen = 0;

	struct fly_bllist *__b;
	fly_for_each_bllist(__b, &__c->method_chain){
		c = fly_bllist_data(__b, struct fly_http_method, blelem);
		vallen += strlen(c->name);
		if (__b->next != &__c->method_chain)
			vallen += strlen(", ");
	}
	value = fly_pballoc(ci->pool, sizeof(fly_hdr_value)*(vallen+1));
	ptr = value;

	fly_for_each_bllist(__b, &__c->method_chain){
		c = fly_bllist_data(__b, struct fly_http_method, blelem);
		memcpy(ptr, c->name, strlen(c->name));
		ptr += strlen(c->name);
		if (__b->next != &__c->method_chain){
			memcpy(ptr , ", ", strlen(", "));
			ptr += strlen(", ");
		}
	}
	value[vallen] = '\0';

	/* add header */
	return fly_header_add(ci, fly_header_name_length("Allow"), fly_header_value_length(value));
}

int fly_add_server(fly_hdr_ci *ci, bool hv2)
{
	if (hv2)
		return fly_header_add_ver_ifno(ci, fly_header_name_length("server"), fly_header_value_length(FLY_SERVER_NAME), hv2);
	else
		return fly_header_add_ifno(ci, fly_header_name_length("Server"), fly_header_value_length(FLY_SERVER_NAME));
}

fly_hdr_c *fly_header_chain_debug(struct fly_bllist *__b)
{
	fly_hdr_c *__c;
	__c = fly_bllist_data(__b, fly_hdr_c, blelem);
	return __c;
}

void fly_header_state(fly_hdr_ci *__ci, struct fly_request *__req)
{
	if (is_fly_request_http_v2(__req))
		__ci->state = __req->stream->state;
}

void fly_response_header_init(struct fly_response *__res, struct fly_request *__req)
{
	if (!__res->header){
		__res->header = fly_header_init(__req->ctx);
		fly_header_state(__res->header, __req);
	}
}

