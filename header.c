#include <stdio.h>
#include <time.h>
#include <string.h>
#include <stdlib.h>
#include "header.h"
#include "body.h"
#include "mime.h"

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

char *fly_get_header_lines_ptr(char *buffer)
{
	char *header;
	char *end_of_reqline;

	end_of_reqline = strstr(buffer, "\r\n");
	if (end_of_reqline == NULL)
		return NULL;
	header = end_of_reqline + FLY_CRLF_LENGTH;
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

/*
 *		Builtin Header Function
 *
 */
int fly_date_header(fly_hdr_ci *ci)
{
	#define FLY_DATE_LENGTH			50
	time_t now;
	int length;
	char value_field[FLY_DATE_LENGTH];
	now = time(NULL);

	if (now == -1)
		return -1;
    length = strftime(
		value_field,
		(size_t) FLY_DATE_LENGTH,
		"%a, %d %b %Y %H:%M:%S GMT",
		gmtime(&now)
	);
	if (length == 0)
		return -1;

	value_field[length] = '\0';
	return fly_header_add(ci, fly_header_name_length("Date"), fly_header_value_length(value_field));
	#undef FLY_DATE_LENGTH
}

int fly_content_type_header(fly_hdr_ci *ci, fly_mime_e type)
{
	#define FLY_CONTENT_TYPE_LENGTH		100
	fly_mime_t *mime;

	mime = fly_mime_from_type(type);
	if (mime == NULL || mime->name == NULL)
		return -1;

	return fly_header_add(ci, fly_header_name_length("Content-Type"), fly_header_value_length(mime->name));
	#undef FLY_CONTENT_TYPE_LENGTH
}

int fly_content_length_heaedr(fly_hdr_ci *ci, fly_body_t *body)
{
	#define FLY_CONTENT_LENGTH_LENGTH	100
	char contlen_str[FLY_CONTENT_LENGTH_LENGTH];
	if (body == NULL)
		return -1;

	if (snprintf(contlen_str, FLY_CONTENT_LENGTH_LENGTH, "%d", body->body_len) == -1)
		return -1;

	return fly_header_add(ci, fly_header_name_length("Content-Length"), fly_header_value_length(contlen_str));
	#undef FLY_CONTENT_LENGTH_LENGTH
}
