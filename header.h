#ifndef _HEADER_H
#define _HEADER_H
#include <string.h>
#include "alloc.h"
#include "util.h"

typedef char fly_hdr_value;
typedef char fly_hdr_name;

#define FLY_STATUS_LINE_MAX		50
#define FLY_HEADER_NAME_MAX		20
#define FLY_HEADER_LINE_MAX		100
#define FLY_HEADER_ELES_MAX		1000
#define FLY_REQHEADER_POOL_SIZE	10 
#define fly_name_hdr_gap()		": "
#define FLY_HEADER_VALUE_MAX	(FLY_HEADER_LINE_MAX-FLY_HEADER_NAME_MAX-strlen(fly_name_hdr_gap()))

#define FLY_NAME	"fly-server"
#define fly_server_name()	(FLY_NAME)

#define FLY_HEADER_POOL_PAGESIZE		2
struct fly_hdr_chain{
	fly_hdr_name *name;
	fly_hdr_value *value;
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

char *fly_get_header_lines_ptr(char *buffer);

#endif
