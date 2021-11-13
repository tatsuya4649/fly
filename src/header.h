#ifndef _HEADER_H
#define _HEADER_H
#include <string.h>
#include <sys/stat.h>
#include "alloc.h"
#include "util.h"
#include "mount.h"
#include "buffer.h"

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
	size_t name_len;
	size_t value_len;
	struct fly_bllist blelem;

	/* for HTTP2 */
	int index;
	size_t hname_len;
	size_t hvalue_len;
	fly_hdr_value *hen_name;
	fly_hdr_name *hen_value;
	int index_update;
	fly_bit_t name_index: 1;
	fly_bit_t static_table: 1;
	fly_bit_t dynamic_table: 1;
	fly_bit_t huffman_name: 1;
	fly_bit_t huffman_value: 1;

	/* for cookie */
	fly_bit_t cookie: 1;
};

struct fly_hv2_state;
struct fly_hdr_chain_info{
	fly_pool_t *pool;
	struct fly_bllist	chain;
	unsigned chain_count;

	/* for HTTP2 */
	struct fly_hv2_state *state;
};
typedef struct fly_hdr_chain fly_hdr_c;
typedef struct fly_hdr_chain_info fly_hdr_ci;

#ifdef DEBUG
fly_hdr_c *fly_header_chain_debug(struct fly_bllist *__b);
#endif

struct fly_context;
fly_hdr_ci *fly_header_init(struct fly_context *ctx);
void fly_header_release(fly_hdr_ci *info);
int fly_header_add(fly_hdr_ci *chain_info, fly_hdr_name *name, size_t name_len, fly_hdr_value *value, size_t value_len);
int fly_header_add_ifno(fly_hdr_ci *chain_info, fly_hdr_name *name, size_t name_len, fly_hdr_value *value, size_t value_len);
int fly_header_add_ver(fly_hdr_ci *ci, fly_hdr_name *name, size_t name_len, fly_hdr_value *value, size_t value_len, bool hv2);
int fly_header_add_ver_ifno(fly_hdr_ci *ci, fly_hdr_name *name, size_t name_len, fly_hdr_value *value, size_t value_len, bool hv2);
fly_hdr_c *fly_header_addc(fly_hdr_ci *chain_info, fly_hdr_name *name, int name_len, fly_hdr_value *value, int value_len, bool beginning);
int fly_header_addb(fly_buffer_c *bc, fly_hdr_ci *chain_info, fly_hdr_name *name, int name_len, fly_hdr_value *value, int value_len);
int fly_header_addbv(fly_buffer_c *bc, fly_hdr_ci *chain_info, fly_hdr_name *name, int name_len, fly_hdr_value *value, int value_len);
int fly_header_addmodify(fly_hdr_ci *chain_info, fly_hdr_name *name, int name_len, fly_hdr_value *value, int value_len, bool hv2);
int fly_header_delete(fly_hdr_ci *chain_info, char *name);
char *fly_header_from_chain(fly_hdr_ci *chain_info);
size_t fly_hdrlen_from_chain(fly_hdr_ci *chain_info);

fly_buffer_c *fly_get_header_lines_buf(fly_buffer_t *__buf);
long long fly_content_length(fly_hdr_ci *ci);
int fly_connection(fly_hdr_ci *ci);
#define FLY_CONNECTION_CLOSE			0
#define	FLY_CONNECTION_KEEP_ALIVE		1

struct fly_mount_parts_file;
int fly_add_content_length_from_stat(fly_hdr_ci *ci, struct stat *sb, bool lower);
int fly_add_content_length_from_fd(fly_hdr_ci *ci, int fd, bool lower);
int fly_add_content_etag(fly_hdr_ci *ci, struct fly_mount_parts_file *pf, bool lower);
int fly_add_last_modified(fly_hdr_ci *ci, struct fly_mount_parts_file *pf, bool lower);

int fly_add_date(fly_hdr_ci *ci, bool lower);
#include "mime.h"
int fly_add_content_type(fly_hdr_ci *ci, fly_mime_type_t *type, bool lower);
enum fly_header_connection_e{
	KEEP_ALIVE,
	CLOSE,
};
int fly_add_connection(fly_hdr_ci *ci, enum fly_header_connection_e connection);
#include "encode.h"
int fly_add_content_encoding(fly_hdr_ci *ci, struct fly_encoding_type *e, bool hv2);
int fly_add_content_length(fly_hdr_ci *ci, size_t cl, bool hv2);
fly_hdr_value *fly_content_encoding(fly_hdr_ci *ci);
fly_hdr_value *fly_content_encoding_s(fly_hdr_ci *ci);
struct fly_request;
int fly_add_allow(fly_hdr_ci *ci, struct fly_request *req);
int fly_add_server(fly_hdr_ci *ci, bool hv2);

struct fly_response;
struct fly_request;
void fly_header_state(fly_hdr_ci *__ci, struct fly_request *__req);
void fly_response_header_init(struct fly_response *__res, struct fly_request *__req);
bool fly_is_multipart_form_data(fly_hdr_ci *ci);

#define FLY_COOKIE_HEADER_NAME			"Cookie"
#define FLY_COOKIE_HEADER_NAME_S		"cookie"
#define FLY_COOKIE_HEADER_NAME_LEN		6
bool fly_is_cookie(char *name, size_t len);
bool fly_is_cookie_chain(fly_hdr_c *__c);
void fly_check_cookie(fly_hdr_ci *__ci);

#endif
