#ifndef _HEADER_H
#define _HEADER_H

typedef char fly_hdr_value;
typedef char fly_hdr_name;
typedef fly_hdr_value *fly_header_trigger(void);
typedef void fly_header_release(fly_hdr_value *value_field);

#define FLY_HEADER_NAME_MAX		20
#define FLY_HEADER_LINE_MAX		100
#define FLY_HEADER_ELES_MAX		1000
struct fly_hdr_elem{
	fly_hdr_name name[FLY_HEADER_NAME_MAX];
	fly_header_trigger *trig;
	fly_header_release *release;
	struct fly_hdr_elem *next;
};

#define fly_name_hdr_gap()		" : "

extern struct fly_hdr_elem *init_header;
int fly_hdr_init(void);

int fly_register_header(fly_hdr_name *, fly_header_trigger *, fly_header_release *);
int fly_unregister_header(fly_hdr_name *name);
#define DATE_FORMAT		"%a, %d %b %Y %H:%M:%S GMT"
#define DATE_FIELD_LENGTH	40

fly_hdr_value *fly_date_header(void);
void fly_date_header_release(fly_hdr_value *value_field);

#define FLY_NAME	"fly-server"
#define fly_server_name()	(FLY_NAME)

char **fly_hdr_eles_to_string(struct fly_hdr_elem *elems);
void fly_header_free(char **, struct fly_hdr_elem *elem);

#endif
