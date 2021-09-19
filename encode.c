#include "encode.h"

__fly_static fly_encoding_type_t __fly_encodes[] = {
	FLY_ENCODE_TYPE(gzip, 100),
	{ fly_gzip, "x-gzip", 90, fly_gzip_encode, fly_gzip_decode },
//	FLY_ENCODE_TYPE(compress, 50),
//	{ fly_compress, "x-compress", 30 },
	FLY_ENCODE_TYPE(deflate, 75),
	FLY_ENCODE_TYPE(identity, 1),
//	FLY_ENCODE_TYPE(br, 30),
	FLY_ENCODE_ASTERISK,
	FLY_ENCODE_NULL
};

#include "header.h"
#include "request.h"

__fly_static int __fly_accept_encoding(fly_hdr_ci *ci, fly_hdr_c **accept_encoding);
__fly_static int __fly_add_accept_encoding(fly_encoding_t *enc, struct __fly_encoding *ne);
static inline int __fly_quality_value(struct __fly_encoding *e, int qvalue);
__fly_static int __fly_add_accept_encode_asterisk(fly_request_t *req);
static inline fly_encoding_type_t *__fly_asterisk(void);
#define __FLY_PARSE_ACCEPT_ENCODING_PARSEERROR		0
#define __FLY_PARSE_ACCEPT_ENCODING_SUCCESS			1
#define __FLY_PARSE_ACCEPT_ENCODING_ERROR			-1
__fly_static int __fly_parse_accept_encoding(fly_request_t *req, fly_hdr_c *ae_header);
static void __fly_memcpy_name(char *dist, char *src, size_t maxlen);
static inline bool __fly_number(char c);
static inline bool __fly_vchar(char c);
static inline bool __fly_tchar(char c);
static inline bool __fly_alnum(char c);
static inline bool __fly_delimit(char c);
static inline bool __fly_space(char c);
static inline bool __fly_semicolon(char c);
static inline bool __fly_q(char c);
static inline bool __fly_one(char c);
static inline bool __fly_zero(char c);
static inline bool __fly_zeros(char c);
static inline bool __fly_point(char c);
static inline bool __fly_comma(char c);
static inline bool __fly_equal(char c);
static int __fly_quality_value_from_str(char *qvalue);
static int __fly_decide_encoding(fly_encoding_t *__e);

static inline fly_encoding_type_t *__fly_asterisk(void)
{
	for (fly_encoding_type_t *e=__fly_encodes; e->name; e++){
		if (strcmp(e->name, "*") == 0)
			return e;
	}
	return NULL;
}

static inline fly_encoding_type_t *__fly_most_priority(void)
{
	fly_encoding_type_t *most = NULL;

	for (fly_encoding_type_t *e=__fly_encodes; e->name; e++){
		if (most != NULL ? (e->priority > most->priority): true)
			most = e;
	}
	return most;
}

fly_encname_t *fly_encname_from_type(fly_encoding_e type)
{
	for (fly_encoding_type_t *e=__fly_encodes; !FLY_ENCODE_END(e); e++){
		if (e->type == type)
			return e->name;
	}
	return NULL;
}

fly_encoding_type_t *fly_encoding_from_type(fly_encoding_e type)
{
	for (fly_encoding_type_t *e=__fly_encodes; !FLY_ENCODE_END(e); e++){
		if (e->type == type)
			return e;
	}
	return NULL;
}

fly_encoding_type_t *fly_encoding_from_name(fly_encname_t *name)
{
	#define FLY_ENCODE_NAME_LENGTH		20
	fly_encname_t enc_name[FLY_ENCODE_NAME_LENGTH];
	fly_encname_t *ptr;

	ptr = enc_name;
	for (; *name; name++)
		*ptr++ = tolower(*name);
	*ptr = '\0';

	for (fly_encoding_type_t *e=__fly_encodes; !FLY_ENCODE_END(e); e++){
		if (strcmp(e->name, enc_name) == 0)
			return e;
	}
	return NULL;
	#undef FLY_ENCODE_NAME_LENGTH
}

int fly_gzip_decode(fly_encbuf_t *encbuf, size_t encbuflen, fly_encbuf_t *decbuf, size_t decbuflen)
{
	int status;
	z_stream __zstream;

	if (encbuf == NULL || !encbuflen || decbuf == NULL || !decbuflen)
		return FLY_ENCODE_ERROR;

	__zstream.zalloc = Z_NULL;
	__zstream.zfree = Z_NULL;
	__zstream.opaque = Z_NULL;

	__zstream.next_in = Z_NULL;
	__zstream.avail_in = 0;
	if (inflateInit2(&__zstream, 47) != Z_OK)
		return -1;

	__zstream.next_out = decbuf;
	__zstream.avail_out = decbuflen;

	status = Z_OK;

	while(status != Z_STREAM_END){
		if (__zstream.avail_in == 0){
			/* point to encoded buf */
			__zstream.next_in = encbuf;
			__zstream.avail_in = encbuflen;
		}
		status = inflate(&__zstream, Z_NO_FLUSH);
		if (status == Z_STREAM_END)
			break;
		if (status != Z_OK)
			return FLY_ENCODE_ERROR;
		if (__zstream.avail_out == 0)
			return FLY_ENCODE_OVERFLOW;
	}
	if (inflateEnd(&__zstream) != Z_OK)
		return FLY_ENCODE_ERROR;
	return 0;
}

int fly_gzip_encode(fly_encbuf_t *encbuf, size_t encbuflen, fly_encbuf_t *decbuf, size_t decbuflen) { int status, flush; z_stream __zstream; if (encbuf == NULL || !encbuflen || decbuf == NULL || !decbuflen)
		return FLY_ENCODE_ERROR;

	__zstream.zalloc = Z_NULL;
	__zstream.zfree = Z_NULL;
	__zstream.opaque = Z_NULL;

	if (deflateInit2(&__zstream, Z_DEFAULT_COMPRESSION, Z_DEFLATED, 31, 8, Z_DEFAULT_STRATEGY) != Z_OK)
		return -1;

	__zstream.avail_in = 0;
	__zstream.next_out = encbuf;
	__zstream.avail_out = encbuflen;

	flush = Z_NO_FLUSH;
	while(status != Z_STREAM_END){
		if (__zstream.avail_in == 0){
			/* point to encoded buf */
			__zstream.next_in = encbuf;
			__zstream.avail_in = encbuflen;
		}
		status = deflate(&__zstream, flush);
		if (status == Z_STREAM_END)
			break;
		if (status != Z_OK)
			return FLY_ENCODE_ERROR;
		if (__zstream.avail_out == 0)
			return FLY_ENCODE_OVERFLOW;
	}
	if (deflateEnd(&__zstream) != Z_OK)
		return FLY_ENCODE_ERROR;
	return 0;
}

int fly_deflate_decode(fly_encbuf_t *encbuf, size_t encbuflen, fly_encbuf_t *decbuf, size_t decbuflen)
{
	int status;
	z_stream __zstream;

	if (encbuf == NULL || !encbuflen || decbuf == NULL || !decbuflen)
		return FLY_ENCODE_ERROR;

	__zstream.zalloc = Z_NULL;
	__zstream.zfree = Z_NULL;
	__zstream.opaque = Z_NULL;

	__zstream.next_in = Z_NULL;
	__zstream.avail_in = 0;
	if (inflateInit(&__zstream) != Z_OK)
		return -1;

	__zstream.next_out = decbuf;
	__zstream.avail_out = decbuflen;

	status = Z_OK;

	while(status != Z_STREAM_END){
		if (__zstream.avail_in == 0){
			/* point to encoded buf */
			__zstream.next_in = encbuf;
			__zstream.avail_in = encbuflen;
		}
		status = inflate(&__zstream, Z_NO_FLUSH);
		if (status == Z_STREAM_END)
			break;
		if (status != Z_OK)
			return FLY_ENCODE_ERROR;
		if (__zstream.avail_out == 0)
			return FLY_ENCODE_OVERFLOW;
	}
	if (inflateEnd(&__zstream) != Z_OK)
		return FLY_ENCODE_ERROR;
	return 0;
}

int fly_deflate_encode(fly_encbuf_t *encbuf, size_t encbuflen, fly_encbuf_t *decbuf, size_t decbuflen)
{
	int status, flush;
	z_stream __zstream;

	if (encbuf == NULL || !encbuflen || decbuf == NULL || !decbuflen)
		return FLY_ENCODE_ERROR;

	__zstream.zalloc = Z_NULL;
	__zstream.zfree = Z_NULL;
	__zstream.opaque = Z_NULL;

	if (deflateInit(&__zstream, Z_DEFAULT_COMPRESSION) != Z_OK)
		return -1;

	__zstream.avail_in = 0;
	__zstream.next_out = encbuf;
	__zstream.avail_out = encbuflen;

	flush = Z_NO_FLUSH;
	while(status != Z_STREAM_END){
		if (__zstream.avail_in == 0){
			/* point to encoded buf */
			__zstream.next_in = encbuf;
			__zstream.avail_in = encbuflen;
		}
		status = deflate(&__zstream, flush);
		if (status == Z_STREAM_END)
			break;
		if (status != Z_OK)
			return FLY_ENCODE_ERROR;
		if (__zstream.avail_out == 0)
			return FLY_ENCODE_OVERFLOW;
	}
	if (deflateEnd(&__zstream) != Z_OK)
		return FLY_ENCODE_ERROR;
	return 0;
}

int fly_identity_decode(fly_encbuf_t *encbuf, size_t encbuflen, fly_encbuf_t *decbuf, size_t decbuflen)
{
	memset(decbuf, '\0', decbuflen);
	memcpy(decbuf, encbuf, encbuflen);
	return 0;
}

int fly_identity_encode(fly_encbuf_t *encbuf, size_t encbuflen, fly_encbuf_t *decbuf, size_t decbuflen)
{
	memset(encbuf, '\0', encbuflen);
	memcpy(encbuf, decbuf, decbuflen);
	return 0;
}


__fly_static int __fly_accept_encoding(fly_hdr_ci *ci, fly_hdr_c **accept_encoding)
{
#define __FLY_ACCEPT_ENCODING_NOTFOUND		0
#define __FLY_ACCEPT_ENCODING_FOUND			1
#define __FLY_ACCEPT_ENCODING_ERROR			-1
	if (ci->chain_length == 0)
		return __FLY_ACCEPT_ENCODING_NOTFOUND;

	for (fly_hdr_c *c=ci->entry; c!=NULL; c=c->next){
		if (strcmp(c->name, FLY_ACCEPT_ENCODING_HEADER) == 0 && c->value != NULL){
			*accept_encoding = c;
			return __FLY_ACCEPT_ENCODING_FOUND;
		}
	}
	return __FLY_ACCEPT_ENCODING_NOTFOUND;
}

__fly_static int __fly_encode_init(fly_request_t *req)
{
	fly_encoding_t *enc;
	fly_pool_t *pool;

	pool = req->pool;
	enc = fly_pballoc(pool, sizeof(fly_encoding_t));

	if (fly_unlikely_null(enc))
		return -1;

	enc->pool = pool;
	enc->actqty = 0;
	enc->request = req;
	enc->accepts = NULL;
	req->encoding = enc;
	return 0;
}

#define __fly_incencoding(enc)			((enc)->actqty++)
__fly_static int __fly_add_accept_encoding(fly_encoding_t *enc, struct __fly_encoding *ne)
{
	if (!enc || !ne)
		return -1;

	ne->encoding = enc;
	if (enc->accepts == NULL){
		enc->accepts = ne;
		ne->next = NULL;
	}else{
		struct __fly_encoding *e, *prev;
		for (e=enc->accepts; e->next!=NULL; e=e->next){
			/* if same enum __fly_encoding_type, overwrite */
			if (e->type->type == ne->type->type){
				ne->next = e->next;
				if (prev)
					prev->next = ne;
				/* release e */
				fly_pbfree(e->encoding->pool, e);
				goto increment;
			}
			prev = e;
		}
		e->next = ne;
	}

increment:
	__fly_incencoding(enc);
	return 0;
}

__fly_static inline int __fly_quality_value(struct __fly_encoding *e, int qvalue)
{
	/* 0~100% */
	if (qvalue < 0 || qvalue > 100)
		return -1;
	e->quality_value = qvalue;
	return 0;
}

__fly_static void __fly_memcpy_name(char *dist, char *src, size_t maxlen)
{
	size_t i=0;
	while(i++ < maxlen){
		*dist++ = *src++;
		if (__fly_space(*src) || __fly_semicolon(*src) || __fly_comma(*src)){
			*dist = '\0';
			return;
		}
	}
}

__fly_static int __fly_add_accept_encode_asterisk(fly_request_t *req)
{
	struct __fly_encoding *__e;
	fly_pool_t *pool;

	pool = req->pool;
	__e = fly_pballoc(pool, sizeof(struct __fly_encoding));
	if (fly_unlikely_null(__e))
		return -1;

	__e->type = __fly_asterisk();
	__e->quality_value = 100;
	__e->next = NULL;
	if (__e->type == NULL)
		return -1;

	return __fly_add_accept_encoding(req->encoding, __e);
}

int fly_accept_encoding(fly_request_t *req)
{
	fly_hdr_ci *header;
	fly_hdr_c  *accept_encoding;

	header = req->header;
	if (req == NULL || req->pool == NULL || req->header == NULL)
		return -1;

	if (__fly_encode_init(req) == -1)
		return -1;

	switch (__fly_accept_encoding(header, &accept_encoding)){
	case __FLY_ACCEPT_ENCODING_ERROR:
		req->encoding = NULL;
		return -1;
	case __FLY_ACCEPT_ENCODING_NOTFOUND:
		if(__fly_add_accept_encode_asterisk(req) == -1)
			return -1;
		return 0;
	case __FLY_ACCEPT_ENCODING_FOUND:
		return __fly_parse_accept_encoding(req, accept_encoding);
	default:
		return -1;
	}
	FLY_NOT_COME_HERE
}

static inline bool __fly_ualpha(char c)
{
	return (c >= 0x41 && c <= 0x5A) ? true : false;
}

static inline bool __fly_lalpha(char c)
{
	return (c >= 0x61 && c <= 0x7A) ? true : false;
}

static inline bool __fly_alpha(char c)
{
	return (__fly_ualpha(c) || __fly_lalpha(c)) ? true : false;
}

static inline char __fly_alpha_lower(char c)
{
	if (__fly_ualpha(c))
		return c-0x20;
	else
		return c;
}


static inline bool __fly_number(char c)
{
	return (c >= 0x30 && c <= 0x39);
}

static inline bool __fly_vchar(char c)
{
	return (c >= 0x21 && c <= 0x7E);
}

static inline bool __fly_tchar(char c)
{
	return (																\
		(__fly_alpha(c) || __fly_number(c) || c == '!' || c == '#' ||		\
		c == '$' || c == '%' || c == '&' || c == 0x27 || c == '*' ||		\
		c == '+' || c == '-' || c == '.' || c == '^' || c == '_' ||			\
		c == '`' || c == '|' || c == '~' || (__fly_vchar(c) &&	c != ';')	\
	) ? true : false);
}

static inline bool __fly_alnum(char c)
{
	return (__fly_alpha(c) || __fly_number(c)) ? true : false;
}

static inline bool __fly_delimit(char c)
{
	return (
		c == 0x22 || c == '(' || c == ')' || c == ',' || c == '/' || \
		c == ':'  || c == ';' || c == '<' || c == '=' || c == '>' || \
		c == '?'  || c == '@' || c == '[' || c == '\\' || c == ']' || \
		c == '{'  || c == '}' \
	) ? true : false;
}

static inline bool __fly_space(char c)
{
	return (c == 0x20 || c == '\t') ? true : false;
}

static inline bool __fly_semicolon(char c)
{
	return (c == 0x3B) ? true : false;
}

static inline bool __fly_q(char c)
{
	return (c == 'q') ? true : false;
}

static inline bool __fly_one(char c)
{
	return (c == '1') ? true : false;
}

static inline bool __fly_zero(char c)
{
	return (c == '0') ? true : false;
}

static inline bool __fly_zeros(char c)
{
	return (c == '\0') ? true : false;
}

static inline bool __fly_point(char c)
{
	return (c == '.') ? true : false;
}

static inline bool __fly_equal(char c)
{
	return (c == '=') ? true : false;
}

static inline bool __fly_comma(char c)
{
	return (c == ',') ? true : false;
}

__fly_static int __fly_parse_ae(fly_encoding_t *e, fly_hdr_value *ae_value)
{
	fly_hdr_value *ptr;
	fly_pool_t *__pool;
	int decimal_places = 0;
	char *name = NULL, *qvalue = NULL;
	enum {
		__FLY_PARSE_AE_INIT,
		__FLY_PARSE_AE_NAME,
		__FLY_PARSE_AE_WEIGHT_SPACE,
		__FLY_PARSE_AE_WEIGHT_SEMICOLON,
		__FLY_PARSE_AE_WEIGHT_SPACE_AFTER,
		__FLY_PARSE_AE_WEIGHT_Q,
		__FLY_PARSE_AE_WEIGHT_EQUAL,
		__FLY_PARSE_AE_ZERO_INT,
		__FLY_PARSE_AE_ONE_INT,
		__FLY_PARSE_AE_ZERO_POINT,
		__FLY_PARSE_AE_ONE_POINT,
		__FLY_PARSE_AE_ZERO_DECIMAL_POINT,
		__FLY_PARSE_AE_ONE_DECIMAL_POINT,
		__FLY_PARSE_AE_LAST_SPACE,
		__FLY_PARSE_AE_COMMA,
		__FLY_PARSE_AE_ADD,
	} pstatus;

	__pool = e->request->pool;
	for (pstatus=__FLY_PARSE_AE_INIT, ptr=ae_value; ptr;){
		if (!__fly_tchar(*ptr) && !__fly_semicolon(*ptr) && !__fly_space(*ptr) && !__fly_zeros(*ptr))
			/* not allowed character */
			return __FLY_PARSE_ACCEPT_ENCODING_PARSEERROR;

		switch(pstatus){
		case __FLY_PARSE_AE_INIT:
			decimal_places = 0;
			if (__fly_tchar(*ptr)){
				pstatus = __FLY_PARSE_AE_NAME;
				name = ptr;
				continue;
			}

			if (__fly_zeros(*ptr)){
				pstatus = __FLY_PARSE_AE_ADD;
				continue;
			}

			return __FLY_PARSE_ACCEPT_ENCODING_PARSEERROR;
		case __FLY_PARSE_AE_NAME:
			if (__fly_space(*ptr)){
				pstatus = __FLY_PARSE_AE_WEIGHT_SPACE;
				continue;
			}
			if (__fly_semicolon(*ptr)){
				pstatus = __FLY_PARSE_AE_WEIGHT_SEMICOLON;
				continue;
			}
			if (__fly_comma(*ptr)){
				pstatus = __FLY_PARSE_AE_COMMA;
				continue;
			}
			if (__fly_zeros(*ptr)){
				pstatus = __FLY_PARSE_AE_ADD;
				continue;
			}
			if (__fly_tchar(*ptr) && !__fly_space(*ptr) && !__fly_semicolon(*ptr))
				break;

			return __FLY_PARSE_ACCEPT_ENCODING_PARSEERROR;
		case __FLY_PARSE_AE_WEIGHT_SPACE:
			if (__fly_space(*ptr))	break;

			if (__fly_semicolon(*ptr)){
				pstatus = __FLY_PARSE_AE_WEIGHT_SEMICOLON;
				continue;
			}

			return __FLY_PARSE_ACCEPT_ENCODING_PARSEERROR;
		case __FLY_PARSE_AE_WEIGHT_SEMICOLON:
			if (__fly_semicolon(*ptr))
				break;
			if (__fly_space(*ptr)){
				pstatus = __FLY_PARSE_AE_WEIGHT_SPACE_AFTER;
				continue;
			}
			if (__fly_q(*ptr)){
				pstatus = __FLY_PARSE_AE_WEIGHT_Q;
				continue;
			}

			return __FLY_PARSE_ACCEPT_ENCODING_PARSEERROR;
		case __FLY_PARSE_AE_WEIGHT_SPACE_AFTER:
			if (__fly_space(*ptr))
				break;
			if (__fly_q(*ptr)){
				pstatus = __FLY_PARSE_AE_WEIGHT_Q;
				continue;
			}
			return __FLY_PARSE_ACCEPT_ENCODING_PARSEERROR;
		case __FLY_PARSE_AE_WEIGHT_Q:
			if (__fly_q(*ptr))
				break;
			if (__fly_equal(*ptr)){
				pstatus = __FLY_PARSE_AE_WEIGHT_EQUAL;
				continue;
			}
			return __FLY_PARSE_ACCEPT_ENCODING_PARSEERROR;
		case __FLY_PARSE_AE_WEIGHT_EQUAL:
			if (__fly_equal(*ptr))
				break;
			if (__fly_one(*ptr)){
				pstatus = __FLY_PARSE_AE_ONE_INT;
				/* start of quality value */
				qvalue = ptr;
				continue;
			}
			if (__fly_zero(*ptr)){
				pstatus = __FLY_PARSE_AE_ZERO_INT;
				/* start of quality value */
				qvalue = ptr;
				continue;
			}
			return __FLY_PARSE_ACCEPT_ENCODING_PARSEERROR;
		case __FLY_PARSE_AE_ONE_INT:
			if (__fly_one(*ptr))
				break;
			if (__fly_space(*ptr)){
				pstatus = __FLY_PARSE_AE_LAST_SPACE;
				continue;
			}
			if (__fly_point(*ptr)){
				pstatus = __FLY_PARSE_AE_ONE_POINT;
				continue;
			}
			if (__fly_zeros(*ptr)){
				pstatus = __FLY_PARSE_AE_ADD;
				continue;
			}
			if (__fly_comma(*ptr)){
				pstatus = __FLY_PARSE_AE_COMMA;
				continue;
			}
			return __FLY_PARSE_ACCEPT_ENCODING_PARSEERROR;
		case __FLY_PARSE_AE_ZERO_INT:
			if (__fly_zero(*ptr))
				break;
			if (__fly_space(*ptr)){
				pstatus = __FLY_PARSE_AE_LAST_SPACE;
				continue;
			}
			if (__fly_point(*ptr)){
				pstatus = __FLY_PARSE_AE_ZERO_POINT;
				continue;
			}
			if (__fly_zeros(*ptr)){
				pstatus = __FLY_PARSE_AE_ADD;
				continue;
			}
			if (__fly_comma(*ptr)){
				pstatus = __FLY_PARSE_AE_COMMA;
				continue;
			}
			return __FLY_PARSE_ACCEPT_ENCODING_PARSEERROR;
		case __FLY_PARSE_AE_ONE_POINT:
			if (__fly_point(*ptr))
				break;
			if (__fly_zero(*ptr)){
				pstatus = __FLY_PARSE_AE_ONE_DECIMAL_POINT;
				continue;
			}

			return __FLY_PARSE_ACCEPT_ENCODING_PARSEERROR;
		case __FLY_PARSE_AE_ZERO_POINT:
			if (__fly_point(*ptr))
				break;
			if (__fly_number(*ptr)){
				pstatus = __FLY_PARSE_AE_ZERO_DECIMAL_POINT;
				continue;
			}

			return __FLY_PARSE_ACCEPT_ENCODING_PARSEERROR;
		case __FLY_PARSE_AE_ONE_DECIMAL_POINT:
			if (__fly_zero(*ptr) && decimal_places++ < 3)
				break;

			if (__fly_zeros(*ptr)){
				pstatus = __FLY_PARSE_AE_ADD;
				continue;
			}

			if (__fly_space(*ptr)){
				pstatus = __FLY_PARSE_AE_LAST_SPACE;
				continue;
			}

			if (__fly_comma(*ptr)){
				pstatus = __FLY_PARSE_AE_COMMA;
				continue;
			}

			return __FLY_PARSE_ACCEPT_ENCODING_PARSEERROR;
		case __FLY_PARSE_AE_ZERO_DECIMAL_POINT:
			if (__fly_number(*ptr) && decimal_places++ < 3)
				break;

			if (__fly_zeros(*ptr)){
				pstatus = __FLY_PARSE_AE_ADD;
				continue;
			}

			if (__fly_space(*ptr)){
				pstatus = __FLY_PARSE_AE_LAST_SPACE;
				continue;
			}

			if (__fly_comma(*ptr)){
				pstatus = __FLY_PARSE_AE_COMMA;
				continue;
			}

			return __FLY_PARSE_ACCEPT_ENCODING_PARSEERROR;
		case __FLY_PARSE_AE_LAST_SPACE:
			if (__fly_space(*ptr))
				break;

			if (__fly_comma(*ptr)){
				pstatus = __FLY_PARSE_AE_COMMA;
				continue;
			}

			if (__fly_zeros(*ptr)){
				pstatus = __FLY_PARSE_AE_ADD;
				continue;
			}

			pstatus = __FLY_PARSE_AE_ADD;
			continue;
		case __FLY_PARSE_AE_COMMA:
			if (__fly_comma(*ptr) || __fly_space(*ptr))
				break;

			pstatus = __FLY_PARSE_AE_ADD;
			continue;
		case __FLY_PARSE_AE_ADD:
			/* add accept encoding */
			{
				struct __fly_encoding *ne;
				fly_encname_t encname[FLY_ENCNAME_MAXLEN];
				ne = fly_pballoc(__pool, sizeof(struct __fly_encoding));
				if (ne == NULL)
					return __FLY_PARSE_ACCEPT_ENCODING_ERROR;

				__fly_memcpy_name(encname, name!=NULL ? name : "*", FLY_ENCNAME_MAXLEN);
				encname[FLY_ENCNAME_MAXLEN-1] = '\0';

				ne->type = fly_encoding_from_name(encname);
				ne->next = NULL;
				ne->use  = false;
				ne->encoding = e;
				ne->quality_value = __fly_quality_value_from_str(qvalue);

				/* only add supported encoding */
				if (ne->type == NULL)
					goto end_of_add;

				if (__fly_add_accept_encoding(e, ne) == -1)
					return __FLY_PARSE_ACCEPT_ENCODING_ERROR;
			}

end_of_add:
			/* to reach end of Accept-Encoding header */
			if (__fly_zeros(*ptr))
				return __FLY_PARSE_ACCEPT_ENCODING_SUCCESS;
			pstatus = __FLY_PARSE_AE_INIT;
			continue;
		default:
			/* unknown status */
			return __FLY_PARSE_ACCEPT_ENCODING_ERROR;
		}
		ptr++;
	}
	return __FLY_PARSE_ACCEPT_ENCODING_SUCCESS;
}

__fly_static int __fly_parse_accept_encoding(fly_request_t *req, fly_hdr_c *ae_header)
{
	if (ae_header == NULL || req->encoding == NULL)
		return __FLY_PARSE_ACCEPT_ENCODING_ERROR;

	fly_hdr_value *ae_value;
	ae_value = ae_header->value;

	switch(__fly_parse_ae(req->encoding, ae_value)){
	case __FLY_PARSE_ACCEPT_ENCODING_SUCCESS:
		break;
	case __FLY_PARSE_ACCEPT_ENCODING_ERROR:
		return __FLY_PARSE_ACCEPT_ENCODING_ERROR;
	case __FLY_PARSE_ACCEPT_ENCODING_PARSEERROR:
		return __FLY_PARSE_ACCEPT_ENCODING_PARSEERROR;
	}

	/* decide to use which encodes */
	return __fly_decide_encoding(req->encoding);
}

__fly_static int __fly_quality_value_from_str(char *qvalue)
{
	char qv_str[FLY_ENCQVALUE_MAXLEN], *qptr;
	double quality_value;
	if (qvalue == NULL || __fly_one(*qvalue))
		return 100;

	qptr = qv_str;
	while(__fly_zero(*qvalue) || __fly_point(*qvalue) || __fly_number(*qvalue))
		*qptr++ = *qvalue++;
	*qptr = '\0';

	quality_value = atof(qv_str);

	if (quality_value < 0.0 || quality_value > 1.0)
		return -1;

	return (int) (quality_value*100);
}


__fly_static int __fly_decide_encoding(fly_encoding_t *__e)
{
	if (__e == NULL || !__e->actqty)
		return 0;

	int max_quality_value = 0;
	struct __fly_encoding *maxt = NULL;
	fly_encoding_type_t *__p;

	for (struct __fly_encoding *accept=__e->accepts; accept; accept=accept->next){
		accept->use = false;
		if ((accept->quality_value > 0)								\
				&& (accept->quality_value > max_quality_value)		\
				&& (maxt != NULL ? accept->type->priority > maxt->type->priority : true)	\
				){
			if (maxt != NULL)
				maxt->use = false;
			max_quality_value = accept->quality_value;
			maxt = accept;
			accept->use = true;
		}
	}

	/* if asterisk, select most priority encoding. */
	if (maxt->type == __fly_asterisk()){
		__p = __fly_most_priority();
		maxt->type = __p;
		maxt->use = true;
	}
	return 0;
}
