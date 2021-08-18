#ifndef _HEADER_H
#define _HEADER_H
#include <string.h>

typedef struct{
	char *body;
	int body_len;
} fly_trig_data;

typedef char fly_hdr_value;
typedef char fly_hdr_name;
/*
	@params:
	fly_hdr_value: allocated value field pointer.
	@return:
	0: success, -1: error(500 return)
 */
typedef int fly_header_trigger(fly_hdr_value *,fly_trig_data *);
//typedef void fly_header_release(fly_hdr_value *value_field);

#define FLY_STATUS_LINE_MAX		50
#define FLY_HEADER_NAME_MAX		20
#define FLY_HEADER_LINE_MAX		100
#define FLY_HEADER_ELES_MAX		1000
struct fly_hdr_elem{
	fly_hdr_name name[FLY_HEADER_NAME_MAX];
	fly_header_trigger *trig;
//	fly_header_release *release;
	struct fly_hdr_elem *next;
};

#define fly_name_hdr_gap()		" : "
#define FLY_HEADER_VALUE_MAX	(FLY_HEADER_LINE_MAX-FLY_HEADER_NAME_MAX-strlen(fly_name_hdr_gap()))

extern struct fly_hdr_elem *init_header;
int fly_hdr_init(void);

int fly_register_header(
	fly_hdr_name *,
	fly_header_trigger *
//	fly_header_release *
);
int fly_unregister_header(fly_hdr_name *name);
#define DATE_FORMAT		"%a, %d %b %Y %H:%M:%S GMT"
#define DATE_FIELD_LENGTH	40

/* Header Date Field */
int fly_date_header(fly_hdr_value *,fly_trig_data *);
//void fly_date_header_release(fly_hdr_value *value_field);
/* Content-Length Date Field */
int fly_content_length_header(fly_hdr_value *value_field, fly_trig_data *data);

#define FLY_NAME	"fly-server"
#define fly_server_name()	(FLY_NAME)

char **fly_hdr_eles_to_string(struct fly_hdr_elem *elems, int *header_len, char *body, int body_len);
void fly_header_free(char **, struct fly_hdr_elem *elem);

fly_hdr_value *fly_hdr_alloc(void);
void fly_hdr_free(fly_hdr_value *);

#endif
