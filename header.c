#include <stdio.h>
#include <time.h>
#include <string.h>
#include <stdlib.h>
#include "header.h"

//fly_hdr_e fly_init_header;
//int fly_hdr_init(void)
//{
//	fly_init_header.pool = fly_create_pool(FLY_REGISTER_HEADER_POOL_SIZE);
//	if (fly_init_header.pool == NULL)
//		return -1;
//	fly_init_header.entry = NULL;
//	return 0;
//}
//int fly_hdr_release(void)
//{
//	return fly_delete_pool(fly_init_header.pool);
//}
//
//int fly_register_header(
//	fly_hdr_name *name,
//	fly_header_trigger *trig
//){
//	fly_hdr_t *reg;
//	fly_hdr_t *elem;
//
//	if (strlen(name) > FLY_HEADER_NAME_MAX)
//		return -1;
//	reg = fly_palloc(fly_init_header.pool, fly_page_convert(sizeof(fly_hdr_t)));
//	if (reg == NULL)
//		return -1;
//
//	strcpy(reg->name, name);
//	reg->trig = trig;
//	reg->next = NULL;
//
//	if (fly_init_header.entry == NULL){
//		fly_init_header.entry = reg;
//	}else{
//		for (elem=fly_init_header.entry; elem->next!=NULL; elem=elem->next)
//			;
//		elem->next = reg;
//	}
//	return 0;
//}
//
//int fly_unregister_header(fly_hdr_name *name)
//{
//	fly_hdr_t *elem;
//	fly_hdr_t *prev;
//
//	if (strlen(name) > FLY_HEADER_NAME_MAX)
//		return -1;
//	for (elem=fly_init_header.entry; elem->next!=NULL; elem=elem->next){
//		if (strcmp(elem->name, name) == 0){
//			prev->next = elem->next;
//		}
//		prev = elem;
//	}
//	return -1;
//}

//int fly_date_header(fly_hdr_value *value_field, __attribute__((unused)) fly_trig_data *data)
//{
//	time_t now;
//	int length;
//	now = time(NULL);
//
//	if (now == -1)
//		return -1;
//    length = strftime(
//		value_field,
//		DATE_FIELD_LENGTH,
//		"%a, %d %b %Y %H:%M:%S GMT",
//		gmtime(&now)
//	);
//	if (length == 0)
//		return -1;
//	else{
//		value_field[length] = '\0';
//	}
//	return 0;
//}
//int fly_content_length_header(fly_hdr_value *value_field, fly_trig_data *data){
//	sprintf(value_field,"%d", data->body_len);
//	return 0;
//}
//
//int fly_connection_close_header(fly_hdr_value *value_field, __unused fly_trig_data *data)
//{
//	strcpy(value_field, "close");
//	return 0;
//}
//
//int fly_connection_keep_alive_header(fly_hdr_value *value_field, __unused fly_trig_data *data){
//	strcpy(value_field, "keep-alive");
//	return 0;
//}
//
//int fly_header_str(char *res, fly_hdr_t *now, fly_trig_data *trig_data)
//{
//	if (!now->name[0])
//		return 0;
//	char *pos = res;
//	int trig_result;
//
//	strcpy(res, now->name);
//	pos += strlen(now->name);
//	strcpy(pos, fly_name_hdr_gap());
//	pos += strlen(fly_name_hdr_gap());
//	fly_hdr_value *value_field = pos;
//	trig_result = now->trig(value_field, trig_data);
//
//	if (trig_result < 0)
//		return -1;
//
//	strcpy(pos, value_field);
//	pos += strlen(value_field);
//	*pos = '\0';
//	return 0;
//} 
//char **fly_hdr_eles_to_string(
//	fly_pool_t *conn_pool,
//	fly_hdr_t *elems,
//	int *header_len,
//	char *body,
//	int body_len
//){
//	char **results = fly_palloc(conn_pool, fly_page_convert(sizeof(char *)*FLY_HEADER_ELES_MAX));
//	fly_hdr_t *now;
//
//	int i=0;
//	/* builtin header */
//	for (now=fly_init_header.entry; now!=NULL; now=now->next){
//		fly_trig_data trig_data = {
//			.body = body,
//			.body_len = body_len
//		};
//		results[i] = fly_palloc(
//			conn_pool,
//			fly_page_convert(sizeof(char)*FLY_HEADER_LINE_MAX)
//		);
//		if (fly_header_str(results[i], now, &trig_data) < 0){
//			return NULL;
//		}
//		i++;
//	}
//	/* user defined header */
//	if (elems != NULL){
//		fly_trig_data trig_data = {
//			.body = body,
//			.body_len = body_len
//		};
//		results[i] = fly_palloc(
//			conn_pool,
//			fly_page_convert(sizeof(char)*FLY_HEADER_LINE_MAX)
//		);
//		for (now=elems; now!=NULL; now=now->next){
//			if (fly_header_str(results[i], now, &trig_data) < 0){
//				return NULL;
//			}
//			i++;
//		}
//	}
//	results[i] = NULL;
//	*header_len = i;
//	return results;
//}

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
	return fly_delete_pool(info->pool);
}

int fly_header_add(fly_hdr_ci *chain_info, fly_hdr_name *name, fly_hdr_value *value)
{
	fly_hdr_c *new_chain;
	new_chain = fly_pballoc(chain_info->pool, sizeof(fly_hdr_c));
	if (new_chain == NULL)
		return -1;
	new_chain->name = name;
	new_chain->value = value;
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

char *fly_get_header_lines_ptr(char *buffer)
{
	return strstr(buffer, "\r\n") + FLY_CRLF_LENGTH;
}
