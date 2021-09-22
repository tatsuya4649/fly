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

fly_hdr_ci *fly_header_init(void)
{
	fly_pool_t *pool = fly_create_pool(FLY_HEADER_POOL_PAGESIZE);
	if (pool == NULL)
		return NULL;

	fly_hdr_ci *chain_info;
	chain_info = fly_pballoc(pool, sizeof(fly_hdr_ci));
	chain_info->pool = pool;
	chain_info->entry = NULL;
	chain_info->last = NULL;
	chain_info->chain_length = 0;
	return chain_info;
}

int fly_header_release(fly_hdr_ci *info)
{
	return fly_delete_pool(&info->pool);
}

/*
 * WARNING: name_len and value_len is length not including end of '\0'.
 */
int fly_header_add(fly_hdr_ci *chain_info, fly_hdr_name *name, int name_len, fly_hdr_value *value, int value_len)
{
	fly_hdr_c *new_chain;
	new_chain = fly_pballoc(chain_info->pool, sizeof(fly_hdr_c));
	if (new_chain == NULL)
		return -1;

	new_chain->name = fly_pballoc(chain_info->pool, name_len+1);
	memcpy(new_chain->name, name, name_len);
	new_chain->value = fly_pballoc(chain_info->pool, value_len+1);
	memcpy(new_chain->value, value, value_len);
	new_chain->name[name_len] = '\0';
	new_chain->value[value_len] = '\0';

	new_chain->next = NULL;
	if (chain_info->chain_length == 0)
		chain_info->entry = new_chain;
	else
		chain_info->last->next = new_chain;

	chain_info->last = new_chain;
	chain_info->chain_length++;
	return 0;
}

int fly_header_addb(fly_buffer_c *bc, fly_hdr_ci *chain_info, fly_hdr_name *name, int name_len, fly_hdr_value *value, int value_len)
{
	fly_hdr_c *new_chain;
	new_chain = fly_pballoc(chain_info->pool, sizeof(fly_hdr_c));
	if (new_chain == NULL)
		return -1;

	new_chain->name = fly_pballoc(chain_info->pool, name_len+1);
	fly_buffer_memcpy(new_chain->name, name, bc, name_len);
	new_chain->value = fly_pballoc(chain_info->pool, value_len+1);
	fly_buffer_memcpy(new_chain->value, value, bc, value_len);
	new_chain->name[name_len] = '\0';
	new_chain->value[value_len] = '\0';

	new_chain->next = NULL;
	if (chain_info->chain_length == 0)
		chain_info->entry = new_chain;
	else
		chain_info->last->next = new_chain;

	chain_info->last = new_chain;
	chain_info->chain_length++;
	return 0;
}

int fly_header_addmodify(fly_hdr_ci *chain_info, fly_hdr_name *name, int name_len, fly_hdr_value *value, int value_len)
{
	fly_hdr_c *new_chain;

	if (chain_info->chain_length){
		for (fly_hdr_c *__c=chain_info->entry; __c; __c=__c->next){
			if (strcmp(__c->name, name) == 0){
				/* release */
				fly_pbfree(chain_info->pool, __c->value);

				__c->value = fly_pballoc(chain_info->pool, value_len+1);
				__c->value[value_len] = '\0';
				memcpy(__c->value, value, value_len);
				return 0;
			}
		}
	}

	/* if no */
	new_chain = fly_pballoc(chain_info->pool, sizeof(fly_hdr_c));
	if (fly_unlikely_null(new_chain))
		return -1;
	new_chain->name = fly_pballoc(chain_info->pool, name_len+1);
	memcpy(new_chain->name, name, name_len);
	new_chain->value = fly_pballoc(chain_info->pool, value_len+1);
	memcpy(new_chain->value, value, value_len);
	new_chain->name[name_len] = '\0';
	new_chain->value[value_len] = '\0';

	new_chain->next = NULL;
	if (chain_info->chain_length == 0)
		chain_info->entry = new_chain;
	else
		chain_info->last->next = new_chain;

	chain_info->last = new_chain;
	chain_info->chain_length++;
	return 0;
}

int fly_header_delete(fly_hdr_ci *chain_info, char *name)
{
	if (chain_info->entry == NULL)
		return 0;

	fly_hdr_c *prev;
	for (fly_hdr_c *chain=chain_info->entry; chain!=NULL; chain=chain->next){
		if (strcmp(chain->name, name) == 0){
			prev->next = chain->next;
			chain_info->chain_length--;
			return 0;
		}
		prev = chain;
	}
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

char *fly_header_from_chain(fly_hdr_ci *chain_info)
{
	if (chain_info == NULL)
		return NULL;
	if (chain_info->entry == NULL)
		return NULL;

	char *chain_str;
	char *ptr;

	chain_str= fly_palloc(chain_info->pool, FLY_HEADER_POOL_PAGESIZE);
	if (chain_str == NULL)
		return NULL;

	ptr = chain_str;
	for (fly_hdr_c *c=chain_info->entry; c!=NULL; c=c->next){
		ptr = fly_chain_string(ptr, c, chain_str+ (int) fly_byte_convert(FLY_HEADER_POOL_PAGESIZE));
		if (ptr == NULL)
			return NULL;
	}
	*ptr = '\0';
	return chain_str;
}

size_t fly_hdrlen_from_chain(fly_hdr_ci *chain_info)
{
	char *chain_str;
	chain_str = fly_header_from_chain(chain_info);
	if (chain_str == NULL)
		return 0;

	return strlen(chain_str);
}

char *fly_get_header_lines_ptr(fly_buffer_c *__c)
{
	char *header;

	header = fly_buffer_strstr_after(__c, FLY_CRLF);
	if (header == NULL)
		return NULL;
	return *header != '\0' ? header : NULL;
}

long long fly_content_length(fly_hdr_ci *ci)
{
	if (ci->chain_length == 0)
		return 0;

	for (fly_hdr_c *c=ci->entry; c!=NULL; c=c->next){
		if (strcmp(c->name, "Content-Length") == 0){
			return c->value != NULL ? atoll(c->value) : 0;
		}
	}
	return 0;
}

int fly_connection(fly_hdr_ci *ci)
{
	if (ci->chain_length == 0)
		return -1;

	fly_hdr_c *c;
	for (c=ci->entry; c!=NULL; c=c->next){
		if (strcmp(c->name, "Connection") == 0)
			goto parse_connection;
	}
	return FLY_CONNECTION_CLOSE;

parse_connection:
	if (c->value == NULL)
		return FLY_CONNECTION_CLOSE;

	if (strcmp(c->value, "close") == 0){
		return FLY_CONNECTION_CLOSE;
	}else if (strcmp(c->value, "keep-alive") == 0){
		return FLY_CONNECTION_KEEP_ALIVE;
	}

	return FLY_CONNECTION_CLOSE;
}

fly_hdr_value *fly_content_encoding(fly_hdr_ci *ci)
{
	if (ci->chain_length == 0)
		return NULL;

	fly_hdr_c *__c;
	for (__c=ci->entry; __c; __c=__c->next){
		if (strcmp(__c->name, "Content-Encoding") == 0){
			if (!__c->value)
				return NULL;

			return __c->value;
		}
	}

	return NULL;
}

/*
 *		Builtin Header Function
 *
 */
#include "ftime.h"
int fly_add_date(fly_hdr_ci *ci)
{
	time_t now;
	char value_field[FLY_DATE_LENGTH];

	now = time(NULL);
	if (now == -1)
		return -1;
	if (fly_imt_fixdate(value_field, FLY_DATE_LENGTH, &now))
		return -1;

	return fly_header_add(ci, fly_header_name_length("Date"), fly_header_value_length(value_field));
}

int fly_add_content_type(fly_hdr_ci *ci, fly_mime_type_t *type)
{
	if (fly_unlikely_null(type) || \
			fly_unlikely_null(type->name || \
			fly_mime_invalid(type)))
		return 0;

	return fly_header_add(ci, fly_header_name_length("Content-Type"), fly_header_value_length(type->name));
}

int fly_add_content_length_from_stat(fly_hdr_ci *ci, struct stat *sb)
{
	char *contlen_str;
	int len;

	len = fly_number_ldigits(sb->st_size)+1;
	contlen_str = fly_pballoc(ci->pool, (sizeof(char)*len));
	if (fly_unlikely_null(contlen_str))
		return -1;

	if (snprintf(contlen_str, len, "%ld", (long) sb->st_size) == -1)
		return -1;
	return fly_header_add(ci, fly_header_name_length("Content-Length"), fly_header_value_length(contlen_str));
}

int fly_add_content_length_from_fd(fly_hdr_ci *ci, int fd)
{
	struct stat sb;

	if (fstat(fd, &sb) == 1)
		return -1;

	return fly_add_content_length_from_stat(ci, &sb);
}

int fly_add_content_etag(fly_hdr_ci *ci, struct fly_mount_parts_file *pf)
{
	return fly_header_add(ci, fly_header_name_length("ETag"), (char *) pf->hash->md5, 2*FLY_MD5_LENGTH);
}

int fly_add_last_modified(fly_hdr_ci *ci, struct fly_mount_parts_file *pf)
{
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

int fly_add_content_encoding(fly_hdr_ci *ci, fly_encoding_t *e)
{
	char *encname;

	encname = fly_decided_encoding_name(e);
	if (encname == NULL)
		return -1;

	return fly_header_add(ci, fly_header_name_length("Content-Encoding"), fly_header_value_length(encname));
}

int fly_add_content_length(fly_hdr_ci *ci, size_t cl)
{
	int len;
	char *contlen_str;

	len = fly_number_ldigits(cl)+1;
	contlen_str = fly_pballoc(ci->pool, (sizeof(char)*len));
	if (fly_unlikely_null(contlen_str))
		return -1;

	if (snprintf(contlen_str, len, "%ld", (long) cl) == -1)
		return -1;
	return fly_header_addmodify(ci, fly_header_name_length("Content-Length"), fly_header_value_length(contlen_str));
}

int fly_add_allow(fly_hdr_ci *ci, fly_request_t *req)
{
	fly_uri_t *uri;
	fly_route_reg_t *route_reg;
	struct fly_http_method_chain *__c, *c;
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
	for (c=__c; c; c=c->next){
		vallen += strlen(c->method->name);
		if (c->next)
			vallen += strlen(", ");
	}
	value = fly_pballoc(ci->pool, sizeof(fly_hdr_value)*(vallen+1));
	ptr = value;
	for (c=__c; c; c=c->next){
		memcpy(ptr, c->method->name, strlen(c->method->name));
		ptr += strlen(c->method->name);
		if (c->next){
			memcpy(ptr , ", ", strlen(", "));
			ptr += strlen(", ");
		}
	}
	value[vallen] = '\0';

	/* add header */
	return fly_header_add(ci, fly_header_name_length("Allow"), fly_header_value_length(value));
}

int fly_add_server(fly_hdr_ci *ci)
{
	return fly_header_add(ci, fly_header_name_length("Server"), fly_header_value_length(FLY_SERVER_NAME));
}
