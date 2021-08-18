#include <stdio.h>
#include <time.h>
#include <string.h>
#include <stdlib.h>
#include "header.h"


struct fly_hdr_elem *init_header;

int fly_hdr_init(void)
{
//	init_header = malloc(sizeof(struct fly_hdr_elem));
//	if (init_header == NULL)
//		return -1;
//	init_header->trig = NULL;
//	init_header->release = NULL;
//	init_header->next = NULL;
	init_header = NULL;
	return 0;
}

int fly_register_header(
	fly_hdr_name *name,
	fly_header_trigger *trig
//	fly_header_release*release
){
	struct fly_hdr_elem *reg;
	struct fly_hdr_elem *elem;
	if (strlen(name) > FLY_HEADER_NAME_MAX)
		return -1;
	reg = malloc(sizeof(struct fly_hdr_elem));
	if (reg == NULL)
		return -1;

	strcpy(reg->name, name);
	reg->trig = trig;
//	reg->release = release;
	reg->next = NULL;

	if (init_header == NULL){
		init_header = reg;
		return 0;
	}

	for (elem=init_header; elem->next!=NULL; elem=elem->next)
		;
	
	elem->next = reg;
	return 0;
}

void fly_header_free(char **lines, struct fly_hdr_elem *elem)
{
	free(elem);

	if (lines != NULL){
		for (char **now=lines; *now!=NULL; now++)
			free(*now);
	}
}

int fly_unregister_header(fly_hdr_name *name)
{
	struct fly_hdr_elem *elem;
	struct fly_hdr_elem *prev;

	if (strlen(name) > FLY_HEADER_NAME_MAX)
		return -1;
	for (elem=init_header; elem->next!=NULL; elem=elem->next){
		if (strcmp(elem->name, name) == 0){
			prev->next = elem->next;
			fly_header_free(NULL, elem);
		}
		prev = elem;
	}
	return -1;
}

fly_hdr_value *fly_hdr_alloc(void)
{
	return (fly_hdr_value *) malloc(FLY_HEADER_VALUE_MAX);
}

void fly_hdr_free(fly_hdr_value *value_field)
{
	free(value_field);
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

int fly_header_str(char **res, struct fly_hdr_elem *now, fly_trig_data *trig_data)
{
	if (!now->name[0])
		return 0;
	*res = malloc(sizeof(char)*FLY_HEADER_LINE_MAX);
	char *pos = *res;
	int trig_result;

	strcpy(*res, now->name);
	pos += strlen(now->name);
	strcpy(pos, fly_name_hdr_gap());
	pos += strlen(fly_name_hdr_gap());
	fly_hdr_value *value_field = fly_hdr_alloc();
	trig_result = now->trig(value_field, trig_data);

	if (trig_result < 0)
		return -1;

	strcpy(pos, value_field);
	pos += strlen(value_field);
	fly_hdr_free(value_field);
	*pos = '\0';

	return 0;
}

char **fly_hdr_eles_to_string(
	struct fly_hdr_elem *elems,
	int *header_len,
	char *body,
	int body_len
){
	char **results = malloc(sizeof(char *)*FLY_HEADER_ELES_MAX);
	struct fly_hdr_elem *now;

	int i=0;
	/* builtin header */
	for (now=init_header; now!=NULL; now=now->next){
		fly_trig_data trig_data = {
			.body = body,
			.body_len = body_len
		};
		if (fly_header_str(&results[i], now, &trig_data) < 0){
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
		for (now=elems; now!=NULL; now=now->next){
			if (fly_header_str(&results[i], now, &trig_data) < 0){
				return NULL;
			}
			i++;
		}
	}
	results[i] = NULL;
	*header_len = i;
	return results;
}
