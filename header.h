#ifndef _HEADER_H
#define _HEADER_H
#include "alloc.h"
#include <string.h>
#include "util.h"

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

#define FLY_STATUS_LINE_MAX		50
#define FLY_HEADER_NAME_MAX		20
#define FLY_HEADER_LINE_MAX		100
#define FLY_HEADER_ELES_MAX		1000
#define FLY_REGISTER_HEADER_POOL_SIZE	10

struct fly_hdr_elem{
	fly_hdr_name name[FLY_HEADER_NAME_MAX];
	fly_header_trigger *trig;
	struct fly_hdr_elem *next;
};
typedef struct fly_hdr_elem fly_hdr_t;

struct fly_hdr{
	fly_pool_t *pool;
	fly_hdr_t *entry;
};
/* entry point of all header */
typedef struct fly_hdr fly_hdr_e;
#define fly_name_hdr_gap()		": "
#define FLY_HEADER_VALUE_MAX	(FLY_HEADER_LINE_MAX-FLY_HEADER_NAME_MAX-strlen(fly_name_hdr_gap()))

extern fly_hdr_e fly_init_header;
//int fly_hdr_init(void);
//int fly_hdr_release(void);

int fly_register_header(
	fly_hdr_name *,
	fly_header_trigger *
);
int fly_unregister_header(fly_hdr_name *name);

/* Header Date Field */
#define DATE_FORMAT		"%a, %d %b %Y %H:%M:%S GMT"
#define DATE_FIELD_LENGTH	40
int fly_date_header(fly_hdr_value *,__unused fly_trig_data *);
/* Content-Length Date Field */
int fly_content_length_header(fly_hdr_value *value_field,__unused fly_trig_data *data);
/* Connection: close */
int fly_connection_close_header(fly_hdr_value *value_field,__unused fly_trig_data *data);
/* Connection: keey-alive */
int fly_connection_keep_alive_header(fly_hdr_value *value_field,__unused fly_trig_data *data);

#define FLY_NAME	"fly-server"
#define fly_server_name()	(FLY_NAME)

char **fly_hdr_eles_to_string(fly_pool_t *pool, fly_hdr_t *elems, int *header_len, char *body, int body_len);

#define FLY_HEADER_POOL_PAGESIZE		2
struct fly_hdr_chain{
	char *name;
	char *value;
	struct fly_hdr_chain *next;
};
struct fly_hdr_chain_info{
	fly_pool_t *pool;
	struct fly_hdr_chain *entry;
	struct fly_hdr_chain *last;
	unsigned chain_length;
};
typedef struct fly_hdr_chain fly_hdr_c;
typedef struct fly_hdr_chain_info fly_hdr_ci;

fly_hdr_ci *fly_header_init(void);
int fly_header_release(fly_hdr_ci *info);
int fly_header_add(fly_hdr_ci *chain_info, char *name, char *value);
int fly_header_delete(fly_hdr_ci *chain_info, char *name);
char *fly_header_from_chain(fly_hdr_ci *chain_info);

#endif
