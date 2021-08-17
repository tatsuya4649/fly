#include <time.h>
#include <string.h>
#include <stdlib.h>
#include "header.h"


struct fly_hdr_elem *init_header;

int fly_hdr_init(void)
{
	init_header = malloc(sizeof(struct fly_hdr_elem));
	if (init_header == NULL)
		return -1;
	init_header->trig = NULL;
	init_header->release = NULL;
	init_header->next = NULL;
	return 0;
}

int fly_register_header(fly_hdr_name *name, fly_header_trigger *trig, fly_header_release*release)
{
	struct fly_hdr_elem *reg;
	struct fly_hdr_elem *elem;
	if (strlen(name) > FLY_HEADER_NAME_MAX)
		return -1;
	reg = malloc(sizeof(struct fly_hdr_elem));
	if (reg == NULL)
		return -1;

	strcpy(reg->name, name);
	reg->trig = trig;
	reg->release = release;
	reg->next = NULL;

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

fly_hdr_value *fly_date_header(void)
{
	time_t now;
	int length;
	fly_hdr_value *field;
	field = malloc(DATE_FIELD_LENGTH);
	now = time(NULL);

	if (now == -1)
		return NULL;
    length = strftime(
		field,
		DATE_FIELD_LENGTH,
		"%a, %d %b %Y %H:%M:%S GMT",
		gmtime(&now)
	);
	if (length == 0)
		return NULL;
	else{
		field[length] = '\0';
	}
	return field;
}


void fly_date_header_release(fly_hdr_value *value_field)
{
	free(value_field);
}


void fly_header_str(char **res, struct fly_hdr_elem *now)
{
	if (!now->name[0])
		return;
	*res = malloc(sizeof(char)*FLY_HEADER_LINE_MAX);
	char *pos = *res;

	strcpy(*res, now->name);
	pos += strlen(now->name);
	strcpy(pos, fly_name_hdr_gap());
	pos += strlen(fly_name_hdr_gap());
	fly_hdr_value *value = now->trig();
	strcpy(pos, value);
	pos += strlen(value);
	now->release(value);
	*pos = '\0';
}

char **fly_hdr_eles_to_string(struct fly_hdr_elem *elems)
{
	char **results = malloc(sizeof(char *)*FLY_HEADER_ELES_MAX);
	struct fly_hdr_elem *now;

	int i=0;
	/* builtin header */
	for (now=init_header; ; now=now->next){
		fly_header_str(results[i], now);
		i++;
		if (now->next==NULL)
			break;
	}
	/* user defined header */
	if (elems != NULL){
		for (now=elems; now->next!=NULL; now=now->next){
			fly_header_str(results[i], now);
			i++;
		}
	}
	results[i] = NULL;
	return results;
}
