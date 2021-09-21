#ifndef _HEADER_H
#define _HEADER_H
#include <string.h>
#include <sys/stat.h>
#include "alloc.h"
#include "util.h"
#include "mount.h"

typedef char fly_hdr_value;
typedef char fly_hdr_name;

#define FLY_STATUS_LINE_MAX		50
#define FLY_HEADER_NAME_MAX		20
#define FLY_HEADER_LINE_MAX		100
#define FLY_HEADER_VALUE_MAX	(FLY_HEADER_LINE_MAX-FLY_HEADER_NAME_MAX-FLY_CRLF_LENGTH)
#define FLY_HEADER_ELES_MAX		1000
#define FLY_REQHEADER_POOL_SIZE	10
#define fly_name_hdr_gap()		": "
#define FLY_DATE_LENGTH			(50)

#define FLY_SERVER_NAME	("fly")
#define fly_server_name()	(FLY_NAME)

#define FLY_HEADER_POOL_PAGESIZE		2
#define fly_header_name_length(str)				(fly_hdr_name *) (str), (int) strlen((str))
#define fly_header_value_length(str)				(fly_hdr_value *) (str), (int) strlen((str))
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
int fly_header_add(fly_hdr_ci *chain_info, fly_hdr_name *name, int name_len, fly_hdr_value *value, int value_len);
int fly_header_addmodify(fly_hdr_ci *chain_info, fly_hdr_name *name, int name_len, fly_hdr_value *value, int value_len);
int fly_header_delete(fly_hdr_ci *chain_info, char *name);
char *fly_header_from_chain(fly_hdr_ci *chain_info);
size_t fly_hdrlen_from_chain(fly_hdr_ci *chain_info);

char *fly_get_header_lines_ptr(char *buffer);
long long fly_content_length(fly_hdr_ci *ci);
int fly_connection(fly_hdr_ci *ci);
#define FLY_CONNECTION_CLOSE			0
#define	FLY_CONNECTION_KEEP_ALIVE		1

struct fly_mount_parts_file;
int fly_add_content_length(fly_hdr_ci *ci, size_t cl);
int fly_add_content_length_from_stat(fly_hdr_ci *ci, struct stat *sb);
int fly_add_content_length_from_fd(fly_hdr_ci *ci, int fd);
int fly_add_content_etag(fly_hdr_ci *ci, struct fly_mount_parts_file *pf);
int fly_add_last_modified(fly_hdr_ci *ci, struct fly_mount_parts_file *pf);

int fly_add_date(fly_hdr_ci *ci);
#include "mime.h"
int fly_add_content_type(fly_hdr_ci *ci, fly_mime_type_t *type);
enum fly_header_connection_e{
	KEEP_ALIVE,
	CLOSE,
};
int fly_add_connection(fly_hdr_ci *ci, enum fly_header_connection_e connection);
#include "encode.h"
int fly_add_content_encoding(fly_hdr_ci *ci, fly_encoding_t *e);
int fly_add_content_length(fly_hdr_ci *ci, size_t cl);
fly_hdr_value *fly_content_encoding(fly_hdr_ci *ci);
struct fly_request;
int fly_add_allow(fly_hdr_ci *ci, struct fly_request *req);
int fly_add_server(fly_hdr_ci *ci);

#endif
