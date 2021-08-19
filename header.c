#include <stdio.h>
#include <time.h>
#include <string.h>
#include <stdlib.h>
#include "header.h"


fly_hdr_e fly_init_header;
int fly_hdr_init(void)
{
	fly_init_header.pool = fly_create_pool(FLY_REGISTER_HEADER_POOL_SIZE);
	if (fly_init_header.pool == NULL)
		return -1;
	fly_init_header.entry = NULL;
	return 0;
}
int fly_hdr_release(void)
{
	return fly_delete_pool(fly_init_header.pool);
}

int fly_register_header(
	fly_hdr_name *name,
	fly_header_trigger *trig
){
	fly_hdr_t *reg;
	fly_hdr_t *elem;

	if (strlen(name) > FLY_HEADER_NAME_MAX)
		return -1;
	reg = fly_palloc(fly_init_header.pool, fly_page_convert(sizeof(fly_hdr_t)));
	if (reg == NULL)
		return -1;

	strcpy(reg->name, name);
	reg->trig = trig;
	reg->next = NULL;

	if (fly_init_header.entry == NULL){
		fly_init_header.entry = reg;
	}else{
		for (elem=fly_init_header.entry; elem->next!=NULL; elem=elem->next)
			;
		elem->next = reg;
	}
	return 0;
}

int fly_unregister_header(fly_hdr_name *name)
{
	fly_hdr_t *elem;
	fly_hdr_t *prev;

	if (strlen(name) > FLY_HEADER_NAME_MAX)
		return -1;
	for (elem=fly_init_header.entry; elem->next!=NULL; elem=elem->next){
		if (strcmp(elem->name, name) == 0){
			prev->next = elem->next;
		}
		prev = elem;
	}
	return -1;
}

int fly_date_header(fly_hdr_value *value_field, __attribute__((unused)) fly_trig_data *data)
{
	time_t now;
	int length;
	now = time(NULL);

	if (now == -1)
		return -1;
    length = strftime(
		value_field,
		DATE_FIELD_LENGTH,
		"%a, %d %b %Y %H:%M:%S GMT",
		gmtime(&now)
	);
	if (length == 0)
		return -1;
	else{
		value_field[length] = '\0';
	}
	return 0;
}

int fly_content_length_header(fly_hdr_value *value_field, fly_trig_data *data){
	return -1;
	if (data->body != NULL)
		return -1;
	sprintf(value_field,"%d", data->body_len);
	return 0;
}

int fly_header_str(char *res, fly_hdr_t *now, fly_trig_data *trig_data)
{
	if (!now->name[0])
		return 0;
	char *pos = res;
	int trig_result;

	strcpy(res, now->name);
	pos += strlen(now->name);
	strcpy(pos, fly_name_hdr_gap());
	pos += strlen(fly_name_hdr_gap());
	fly_hdr_value *value_field = pos;
	trig_result = now->trig(value_field, trig_data);

	if (trig_result < 0)
		return -1;

	strcpy(pos, value_field);
	pos += strlen(value_field);
	*pos = '\0';
	return 0;
} 
char **fly_hdr_eles_to_string(
	fly_hdr_t *elems,
	fly_pool_t *conn_pool,
	int *header_len,
	char *body,
	int body_len
){
	char **results = fly_palloc(conn_pool, fly_page_convert(sizeof(char *)*FLY_HEADER_ELES_MAX));
	fly_hdr_t *now;

	int i=0;
	/* builtin header */
	for (now=fly_init_header.entry; now!=NULL; now=now->next){
		fly_trig_data trig_data = {
			.body = body,
			.body_len = body_len
		};
		results[i] = fly_palloc(
			conn_pool,
			fly_page_convert(sizeof(char)*FLY_HEADER_LINE_MAX)
		);
		if (fly_header_str(results[i], now, &trig_data) < 0){
			return NULL;
		}
		i++;
	}
	/* user defined header */
	if (elems != NULL){
		fly_trig_data trig_data = {
			.body = body,
			.body_len = body_len
		};
		results[i] = fly_palloc(
			conn_pool,
			fly_page_convert(sizeof(char)*FLY_HEADER_LINE_MAX)
		);
		for (now=elems; now!=NULL; now=now->next){
			if (fly_header_str(results[i], now, &trig_data) < 0){
				return NULL;
			}
			i++;
		}
	}
	results[i] = NULL;
	*header_len = i;
	return results;
}
