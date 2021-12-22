#include "encode.h"
#include "response.h"
#include "conf.h"

__fly_static fly_encoding_type_t __fly_encodes[] = {
#ifdef HAVE_ZLIB_H
	FLY_ENCODE_TYPE(gzip, 100),
	{ fly_gzip, "x-gzip", 90, fly_gzip_encode, fly_gzip_decode },
	FLY_ENCODE_TYPE(deflate, 75),
#endif
	FLY_ENCODE_TYPE(identity, 1),
#if defined HAVE_LIBBROTLIDEC && defined HAVE_LIBBROTLIENC
	FLY_ENCODE_TYPE(br, 30),
#endif
	FLY_ENCODE_ASTERISK,
	FLY_ENCODE_NULL
};

#include "header.h"
#include "request.h"

__fly_static int __fly_accept_encoding(fly_hdr_ci *ci, fly_hdr_c **accept_encoding);
__fly_static void __fly_add_accept_encoding(fly_encoding_t *enc, struct __fly_encoding *ne);
static inline int __fly_quality_value(struct __fly_encoding *e, int qvalue);
static void __fly_add_accept_encode_asterisk(fly_request_t *req);
static inline fly_encoding_type_t *__fly_asterisk(void);
#define __FLY_PARSE_ACCEPT_ENCODING_PARSEERROR		0
#define __FLY_PARSE_ACCEPT_ENCODING_SUCCESS			1
#define __FLY_PARSE_ACCEPT_ENCODING_ERROR			-1
#define __FLY_PARSE_ACCEPT_ENCODING_NOT_ACCEPTABLE  -2
__fly_static int __fly_parse_accept_encoding(fly_request_t *req, fly_hdr_c *ae_header);
__fly_static void __fly_memcpy_name(char *dist, char *src, size_t src_len, size_t maxlen);
static inline bool __fly_number(char c);
static inline bool __fly_vchar(char c);
static inline bool __fly_tchar(char c);
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
#define __FLY_DECIDE_ENCODING_SUCCESS			0
#define __FLY_DECIDE_ENCODING_ERROR				-1
#define __FLY_DECIDE_ENCODING_NOT_ACCEPTABLE	-2
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

#ifdef HAVE_ZLIB_H
int fly_gzip_decode(fly_de_t *de)
{
	int status;
	z_stream __zstream;
	fly_buffer_c *chain;

	if (fly_unlikely(de->type == FLY_DE_DECODE))
		return FLY_DECODE_ERROR;

	if ((!de->target_already_alloc && (de->encbuf == NULL)) || de->decbuf == NULL)
		return FLY_DECODE_ERROR;

	__zstream.zalloc = Z_NULL;
	__zstream.zfree = Z_NULL;
	__zstream.opaque = Z_NULL;

	__zstream.next_in = Z_NULL;
	__zstream.avail_in = 0;
	if (inflateInit2(&__zstream, 47) != Z_OK)
		return FLY_DECODE_ERROR;

	__zstream.next_out = fly_buffer_lunuse_ptr(de->decbuf);
	__zstream.avail_out = fly_buf_act_len(fly_buffer_last_chain(de->decbuf));

	status = Z_OK;

	chain = fly_buffer_first_chain(de->encbuf);
	while(status != Z_STREAM_END){
		if (__zstream.avail_in == 0){
			if (de->target_already_alloc){
				__zstream.next_in = (fly_encbuf_t *) de->already_ptr;
				__zstream.avail_in = de->already_len;
			}else{
				if (chain == fly_buffer_first_chain(de->encbuf)){
					__zstream.next_in = NULL;
					__zstream.avail_in = 0;
				}else{
					__zstream.next_in = chain->use_ptr;
					__zstream.avail_in = chain->use_len;
				}
				chain = fly_buffer_next_chain(chain);
			}
		}
		status = inflate(&__zstream, Z_NO_FLUSH);
		if (status == Z_STREAM_END)
			break;

		switch(status){
		case Z_OK:
			break;
		case Z_BUF_ERROR:
			goto buffer_error;
		default:
			goto error;
		}

		if (__zstream.avail_out == 0){
			if (de->target_already_alloc)
				goto buffer_error;

			if (fly_unlikely_null(fly_d_buf_add(de)))
				goto buffer_error;
			__zstream.next_out = fly_buffer_lunuse_ptr(de->decbuf);
			__zstream.avail_out = fly_buffer_lunuse_len(de->decbuf);
		}
	}

	de->end = true;
	if (inflateEnd(&__zstream) != Z_OK)
		return FLY_DECODE_ERROR;
	return FLY_DECODE_SUCCESS;

buffer_error:
	if (inflateEnd(&__zstream) != Z_OK)
		return FLY_DECODE_ERROR;
	return FLY_DECODE_BUFFER_ERROR;
error:
	if (inflateEnd(&__zstream) != Z_OK)
		return FLY_DECODE_ERROR;
	return FLY_DECODE_ERROR;
}

int fly_gzip_encode(fly_de_t *de)
{
	switch (de->type){
	case FLY_DE_DECODE:
		return FLY_ENCODE_TYPE_ERROR;
	case FLY_DE_FROM_PATH:
		if (lseek(de->fd, de->offset, SEEK_SET) == -1)
			return FLY_ENCODE_SEEK_ERROR;
	default:
		break;
	}

	int status, flush;
	size_t contlen = 0;
	z_stream __zstream;
	fly_buffer_c *chain;

	if (de->encbuf == NULL || (de->type != FLY_DE_ENCODE && de->decbuf == NULL)){
#ifdef DEBUG
		printf("GZIP ENCODE ERROR: %s: %d\n", __FILE__, __LINE__);
#endif
		return FLY_ENCODE_ERROR;
	}

	__zstream.zalloc = Z_NULL;
	__zstream.zfree = Z_NULL;
	__zstream.opaque = Z_NULL;

	if (deflateInit2(&__zstream, Z_DEFAULT_COMPRESSION, Z_DEFLATED, 31, 8, Z_DEFAULT_STRATEGY) != Z_OK){
#ifdef DEBUG
		printf("GZIP ENCODE ERROR: %s: %d\n", __FILE__, __LINE__);
#endif
		return FLY_ENCODE_ERROR;
	}

	__zstream.avail_in = 0;
	__zstream.next_out = fly_buffer_first_useptr(de->encbuf);
	__zstream.avail_out = fly_buf_act_len(fly_buffer_first_chain(de->encbuf));

	flush = Z_NO_FLUSH;
	status = Z_OK;

	if (!de->target_already_alloc)
		chain = fly_buffer_first_chain(de->decbuf);
	while(status != Z_STREAM_END){
		if (flush != Z_FINISH && __zstream.avail_in == 0){
			switch(de->type){
			case FLY_DE_ENCODE:
				if (de->target_already_alloc){
					__zstream.next_in = (Bytef *) de->already_ptr;
					__zstream.avail_in = de->already_len;
				}else{
					__zstream.next_in = chain->use_ptr;
					__zstream.avail_in = chain->unuse_ptr-chain->use_ptr;
					fly_update_chain(&chain, __zstream.next_in, __zstream.avail_in);
				}
				break;
			case FLY_DE_FROM_PATH:
				{
					int numread;
					if ((numread=read(de->fd, chain->use_ptr, fly_buf_act_len(chain))) == -1)
						goto error;
					__zstream.next_in = chain->use_ptr;
					__zstream.avail_in = numread;
				}
				break;
			default:
				FLY_NOT_COME_HERE
			}
			if (!de->target_already_alloc && __zstream.avail_in < fly_buf_act_len(chain))
				flush = Z_FINISH;
			else if (de->target_already_alloc)
				flush = Z_FINISH;
		}
		status = deflate(&__zstream, flush);
		if (status == Z_STREAM_END)
			break;

		switch(status){
		case Z_OK:
			break;
		case Z_BUF_ERROR:
			goto buffer_error;
		default:
			goto error;
		}

		if (__zstream.avail_out == 0){
			contlen += fly_buf_act_len(fly_buffer_last_chain(de->encbuf));
			if (fly_update_buffer(de->encbuf, fly_buf_act_len(fly_buffer_last_chain(de->encbuf))) == -1)
				goto buffer_error;

			__zstream.next_out = fly_buffer_lunuse_ptr(de->encbuf);
			__zstream.avail_out = fly_buffer_lunuse_len(de->encbuf);
		}
	}

	fly_buffer_c *__lc = fly_buffer_last_chain(de->encbuf);
	if (fly_update_buffer(de->encbuf, __lc->len-__zstream.avail_out) == -1)
		goto buffer_error;

	__lc = fly_buffer_last_chain(de->encbuf);
	contlen += __lc->len-__zstream.avail_out;

	de->end = true;
	if (deflateEnd(&__zstream) != Z_OK)
		return FLY_ENCODE_ERROR;

	de->contlen = contlen;
	return FLY_ENCODE_SUCCESS;
buffer_error:
#ifdef DEBUG
		printf("GZIP ENCODE ERROR: %s: %d\n", __FILE__, __LINE__);
#endif
	if (deflateEnd(&__zstream) != Z_OK)
		return FLY_ENCODE_ERROR;
	return FLY_ENCODE_BUFFER_ERROR;
error:
#ifdef DEBUG
		printf("GZIP ENCODE ERROR: %s: %d\n", __FILE__, __LINE__);
#endif
	if (deflateEnd(&__zstream) != Z_OK)
		return FLY_ENCODE_ERROR;
	return FLY_ENCODE_ERROR;
}
#endif

#if defined HAVE_LIBBROTLIDEC && defined HAVE_LIBBROTLIENC
/* brotli compress/decompress */
int fly_br_decode(fly_de_t *de)
{
	if (fly_unlikely(de->type == FLY_DE_DECODE))
		return FLY_DECODE_ERROR;

	if ((!de->target_already_alloc && fly_unlikely_null(de->encbuf)) ||  fly_unlikely_null(de->decbuf))
		return FLY_DECODE_ERROR;

	BrotliDecoderState *state;
	size_t available_in, available_out;
	uint8_t *next_in, *next_out;
	size_t contlen = 0;
	fly_buffer_c *chain;

	state = BrotliDecoderCreateInstance(0, 0, NULL);
	if (state == 0)
		return FLY_DECODE_ERROR;

	next_in = NULL;
	available_in = 0;
	next_out = fly_buffer_lunuse_ptr(de->decbuf);
	available_out = fly_buffer_lunuse_len(de->decbuf);

	chain = fly_buffer_first_chain(de->encbuf);
	while(true){
		if (available_in == 0){
			if (de->target_already_alloc){
				next_in = (fly_encbuf_t *) de->already_ptr;
				available_in = de->already_len;
			}else{
				if (chain == fly_buffer_first_chain(de->encbuf)){
					next_in = NULL;
					available_in = 0;
				}else{
					next_in = chain->use_ptr;
					available_in = chain->use_len;
				}
			}
		}
		switch(BrotliDecoderDecompressStream(
			state,
			&available_in,
			(const uint8_t **) &next_in,
			&available_out,
			&next_out,
			NULL
		)){
		case BROTLI_DECODER_RESULT_ERROR:
			goto error;
		case BROTLI_DECODER_RESULT_NEEDS_MORE_INPUT:
			goto end_decode;
		case BROTLI_DECODER_RESULT_NEEDS_MORE_OUTPUT:
			break;
		case BROTLI_DECODER_RESULT_SUCCESS:
			break;
		default:
			FLY_NOT_COME_HERE
		}

		if (available_out == 0){
			if (de->target_already_alloc)
				goto buffer_error;

			contlen += fly_buf_act_len(fly_buffer_last_chain(de->encbuf));
			if (fly_update_buffer(de->decbuf, fly_buf_act_len(fly_buffer_last_chain(de->decbuf))) == -1)
				goto buffer_error;

			next_out = fly_buffer_lunuse_ptr(de->decbuf);
			available_out = fly_buffer_lunuse_len(de->decbuf);
		}
	}

end_decode:
	;

	fly_buffer_c *__lec;
	if (fly_update_buffer(de->decbuf, fly_buf_act_len(fly_buffer_last_chain(de->decbuf))-available_out) == -1)
		return FLY_DECODE_ERROR;

	__lec = fly_buffer_last_chain(de->encbuf);
	contlen += __lec->len-available_out;

	de->contlen = contlen;
	de->end = true;
	BrotliDecoderDestroyInstance(state);
	return 0;
error:
	BrotliDecoderDestroyInstance(state);
	return FLY_DECODE_ERROR;
buffer_error:
	BrotliDecoderDestroyInstance(state);
	return FLY_DECODE_BUFFER_ERROR;
}

int fly_br_encode(fly_de_t *de)
{
	switch (de->type){
	case FLY_DE_DECODE:
		return FLY_ENCODE_ERROR;
	case FLY_DE_FROM_PATH:
		if (lseek(de->fd, de->offset, SEEK_SET) == -1)
			return FLY_ENCODE_ERROR;
	default:
		break;
	}

	BrotliEncoderOperation op;
	BrotliEncoderState *state;
	size_t available_in, available_out;
	uint8_t *next_in, *next_out;
	size_t contlen = 0;
	fly_buffer_c *chain;

	if (fly_unlikely_null(de->encbuf) || ((de->type != FLY_DE_ENCODE) && fly_unlikely_null(de->decbuf)))
		return FLY_ENCODE_ERROR;

	state = BrotliEncoderCreateInstance(0, 0, NULL);
	if (state == 0)
		return FLY_ENCODE_ERROR;

	available_in = 0;
	next_out = fly_buffer_first_useptr(de->encbuf);
	available_out = fly_buf_act_len(fly_buffer_first_chain(de->encbuf));

	op = BROTLI_OPERATION_PROCESS;
	if (!de->target_already_alloc)
		chain = fly_buffer_first_chain(de->decbuf);
	while(op != BROTLI_OPERATION_FINISH){
		if (available_in == 0){
			switch(de->type){
			case FLY_DE_ENCODE:
				if (de->target_already_alloc){
					next_in = (uint8_t *) de->already_ptr;
					available_in = de->already_len;
				}else{
					next_in = chain->use_ptr;
					available_in = chain->unuse_ptr-chain->use_ptr;
					fly_update_chain(&chain, next_in, available_in);
				}
				break;
			case FLY_DE_FROM_PATH:
				{
					int numread = 0;
					if ((numread=read(de->fd, chain->use_ptr, fly_buf_act_len(chain))) == -1)
						goto error;
					next_in = chain->use_ptr;
					available_in = numread;
				}
				break;
			default:
				FLY_NOT_COME_HERE
			}

			if (!de->target_already_alloc && available_in < (size_t) fly_buf_act_len(fly_buffer_last_chain(de->decbuf)))
				op = BROTLI_OPERATION_FINISH;
			else if (de->target_already_alloc)
				op = BROTLI_OPERATION_FINISH;
			else
				op = BROTLI_OPERATION_PROCESS;
		}
		if (BrotliEncoderCompressStream(
				state,
				op,
				&available_in,
				(const uint8_t **) &next_in,
				&available_out,
				&next_out,
				NULL) == BROTLI_FALSE)
			goto error;

		if (BrotliEncoderIsFinished(state) == BROTLI_TRUE)
			break;

		/* lack of output buffer */
		if (available_out == 0){
			next_out = fly_buffer_lunuse_ptr(de->encbuf);
			contlen += fly_buf_act_len(fly_buffer_last_chain(de->encbuf));
			if (fly_update_buffer(de->encbuf, fly_buf_act_len(fly_buffer_last_chain(de->encbuf))) == -1)
				goto buffer_error;

			next_out = fly_buffer_lunuse_ptr(de->encbuf);
			available_out = fly_buffer_lunuse_len(de->encbuf);
		}
	}

	if (fly_update_buffer(de->encbuf, fly_buf_act_len(chain)-available_out) == -1)
		return -1;
	fly_buffer_c *__lc = fly_buffer_last_chain(de->encbuf);
	contlen += __lc->len-available_out;

	de->end = true;
	BrotliEncoderDestroyInstance(state);

	de->contlen = contlen;
	return 0;

error:
	BrotliEncoderDestroyInstance(state);
	return FLY_ENCODE_ERROR;
buffer_error:
	BrotliEncoderDestroyInstance(state);
	return FLY_ENCODE_BUFFER_ERROR;
}
#endif

#ifdef HAVE_ZLIB_H
int fly_deflate_decode(fly_de_t *de)
{
	if (fly_unlikely(de->type == FLY_DE_DECODE))
		return FLY_DECODE_ERROR;

	int status;
	z_stream __zstream;
	fly_buffer_c *chain;
	size_t contlen = 0;

	if ((!de->target_already_alloc && (de->encbuf == NULL)) || de->decbuf == NULL)
		return FLY_DECODE_ERROR;

	__zstream.zalloc = Z_NULL;
	__zstream.zfree = Z_NULL;
	__zstream.opaque = Z_NULL;

	__zstream.next_in = Z_NULL;
	__zstream.avail_in = 0;
	if (inflateInit(&__zstream) != Z_OK)
		return FLY_DECODE_ERROR;

	__zstream.next_out = fly_buffer_lunuse_ptr(de->decbuf);
	__zstream.avail_out = fly_buf_act_len(fly_buffer_last_chain(de->decbuf));

	status = Z_OK;

	chain = fly_buffer_first_ptr(de->encbuf);
	while(status != Z_STREAM_END){
		if (__zstream.avail_in == 0){
			if (de->target_already_alloc){
				__zstream.next_in = (fly_encbuf_t *) de->already_ptr;
				__zstream.avail_in = de->already_len;
			}else{
				if (chain == fly_buffer_first_chain(de->encbuf)){
					/* point to encoded buf */
					__zstream.next_in = NULL;
					__zstream.avail_in = 0;
				} else{
					__zstream.next_in = chain->use_ptr;
					__zstream.avail_in = chain->use_len;
				}
				chain = fly_buffer_next_chain(chain);
			}
		}
		status = inflate(&__zstream, Z_NO_FLUSH);
		if (status == Z_STREAM_END)
			break;

		switch(status){
		case Z_OK:
			break;
		case Z_BUF_ERROR:
			goto buffer_error;
		default:
			goto error;
		}
		if (__zstream.avail_out == 0){
			if (de->target_already_alloc)
				goto buffer_error;

			contlen += fly_buf_act_len(fly_buffer_last_chain(de->encbuf));
			if (fly_update_buffer(de->decbuf, fly_buf_act_len(fly_buffer_last_chain(de->decbuf))) == -1)
				goto buffer_error;
			__zstream.next_out = fly_buffer_lunuse_ptr(de->decbuf);
			__zstream.avail_out = fly_buffer_lunuse_len(de->decbuf);
		}
	}
	de->end = true;
	contlen += (fly_buffer_lunuse_ptr(de->encbuf)-fly_buffer_luse_ptr(de->encbuf));
	de->contlen = contlen;
	if (inflateEnd(&__zstream) != Z_OK)
		return FLY_DECODE_ERROR;
	return FLY_DECODE_SUCCESS;

buffer_error:
	if (inflateEnd(&__zstream) != Z_OK)
		return FLY_DECODE_ERROR;
	return FLY_DECODE_BUFFER_ERROR;
error:
	if (inflateEnd(&__zstream) != Z_OK)
		return FLY_DECODE_ERROR;
	return FLY_DECODE_ERROR;
}

int fly_deflate_encode(fly_de_t *de)
{
	switch (de->type){
	case FLY_DE_DECODE:
		return FLY_ENCODE_ERROR;
	case FLY_DE_FROM_PATH:
		if (lseek(de->fd, de->offset, SEEK_SET) == -1)
			return FLY_ENCODE_ERROR;
	default:
		break;
	}

	int status=0, flush;
	z_stream __zstream;
	fly_buffer_c *chain;
	size_t contlen = 0;

	if (de->encbuf == NULL || (de->type != FLY_DE_ENCODE && de->decbuf == NULL))
		return FLY_ENCODE_ERROR;

	__zstream.zalloc = Z_NULL;
	__zstream.zfree = Z_NULL;
	__zstream.opaque = Z_NULL;

	if (deflateInit(&__zstream, Z_DEFAULT_COMPRESSION) != Z_OK)
		return FLY_ENCODE_ERROR;

	__zstream.avail_in = 0;
	__zstream.next_out = fly_buffer_first_useptr(de->encbuf);
	__zstream.avail_out = fly_buf_act_len(fly_buffer_first_chain(de->encbuf));

	flush = Z_NO_FLUSH;
	if (!de->target_already_alloc)
		chain = fly_buffer_first_chain(de->decbuf);
	while(status != Z_STREAM_END){
		if (flush != Z_FINISH && __zstream.avail_in == 0){
			/* point to encoded buf */
			switch(de->type){
			case FLY_DE_ENCODE:
				if (de->target_already_alloc){
					__zstream.next_in = (Bytef *) de->already_ptr;
					__zstream.avail_in = de->already_len;
				}else{
					__zstream.next_in = chain->use_ptr;
					__zstream.avail_in = chain->unuse_ptr-chain->use_ptr;
					fly_update_chain(&chain, __zstream.next_in, __zstream.avail_in);
				}
				break;
			case FLY_DE_FROM_PATH:
				{
					int numread = 0;
					numread = read(de->fd, chain->use_ptr, fly_buf_act_len(chain));
					if (numread == -1)
						goto error;

					__zstream.next_in = chain->use_ptr;
					__zstream.avail_in = numread;
				}
				break;
			default:
				FLY_NOT_COME_HERE
			}
			if (!de->target_already_alloc && __zstream.avail_in < fly_buffer_luse_len(de->decbuf))
				flush = Z_FINISH;
			else if (de->target_already_alloc)
				flush = Z_FINISH;
		}
		status = deflate(&__zstream, flush);
		if (status == Z_STREAM_END)
			break;

		switch(status){
		case Z_OK:
			break;
		case Z_BUF_ERROR:
			goto buffer_error;
		default:
			goto error;
		}

		if (__zstream.avail_out == 0){
			contlen += fly_buf_act_len(fly_buffer_last_chain(de->encbuf));
			if (fly_update_buffer(de->decbuf, fly_buf_act_len(fly_buffer_last_chain(de->decbuf))) == -1)
				goto error;

			__zstream.next_out = fly_buffer_lunuse_ptr(de->encbuf);
			__zstream.avail_out = fly_buffer_lunuse_len(de->encbuf);
		}
	}

	fly_buffer_c *__lc = fly_buffer_last_chain(de->encbuf);
	if (fly_update_buffer(de->encbuf, __lc->len-__zstream.avail_out) == -1)
		goto buffer_error;

	__lc = fly_buffer_last_chain(de->encbuf);
	contlen += __lc->len-__zstream.avail_out;

	de->end = true;
	if (deflateEnd(&__zstream) != Z_OK)
		return FLY_ENCODE_ERROR;

	de->contlen = contlen;
	return FLY_ENCODE_SUCCESS;

buffer_error:
	if (deflateEnd(&__zstream) != Z_OK)
		return FLY_ENCODE_ERROR;
	return FLY_ENCODE_BUFFER_ERROR;
error:
	if (deflateEnd(&__zstream) != Z_OK)
		return FLY_ENCODE_ERROR;
	return FLY_ENCODE_ERROR;
}
#endif

__fly_unused int fly_identity_decode(__fly_unused fly_de_t *de){ return 0; }
__fly_unused int fly_identity_encode(__fly_unused fly_de_t *de){ return 0; }


__fly_static int __fly_accept_encoding(fly_hdr_ci *ci, fly_hdr_c **accept_encoding)
{
#define __FLY_ACCEPT_ENCODING_NOTFOUND		0
#define __FLY_ACCEPT_ENCODING_FOUND			1
#define __FLY_ACCEPT_ENCODING_ERROR			-1
	if (ci->chain_count == 0)
		return __FLY_ACCEPT_ENCODING_NOTFOUND;

	struct fly_bllist *__b;
	fly_hdr_c *c;

	fly_for_each_bllist(__b, &ci->chain){
		c = fly_bllist_data(__b, fly_hdr_c, blelem);
		if (c->name_len>0 && \
				c->value_len > 0 && \
				strncmp(c->name, FLY_ACCEPT_ENCODING_HEADER, strlen(FLY_ACCEPT_ENCODING_HEADER)) == 0){
			*accept_encoding = c;
			return __FLY_ACCEPT_ENCODING_FOUND;
		}
	}
	return __FLY_ACCEPT_ENCODING_NOTFOUND;
}

__fly_static void __fly_encode_init(fly_request_t *req)
{
	fly_encoding_t *enc;
	fly_pool_t *pool;

	pool = req->pool;
	enc = fly_pballoc(pool, sizeof(fly_encoding_t));
	enc->pool = pool;
	enc->accept_count = 0;
	enc->request = req;
	req->encoding = enc;

	fly_bllist_init(&enc->accepts);
}

__fly_static void __fly_add_accept_encoding(fly_encoding_t *enc, struct __fly_encoding *ne)
{
	ne->encoding = enc;

	fly_bllist_add_tail(&enc->accepts, &ne->blelem);
	enc->accept_count++;
}

__fly_unused __fly_static inline int __fly_quality_value(struct __fly_encoding *e, int qvalue)
{
	/* 0~100% */
	if (qvalue < 0 || qvalue > 100)
		return -1;
	e->quality_value = qvalue;
	return 0;
}

__fly_static void __fly_memcpy_name(char *dist, char *src, size_t src_len, size_t maxlen)
{
	size_t i=0;
	while(i++ < maxlen){
		*dist++ = *src++;
		if (__fly_space(*src) || __fly_semicolon(*src) || \
				__fly_comma(*src) || i>=src_len){
			*dist = '\0';
			return;
		}
	}
}

static void __fly_add_accept_encode_asterisk(fly_request_t *req)
{
	struct __fly_encoding *__e;
	fly_pool_t *pool;

	pool = req->pool;
	__e = fly_pballoc(pool, sizeof(struct __fly_encoding));
	__e->type = __fly_asterisk();
	__e->quality_value = 100;
	__e->use = false;
	__fly_add_accept_encoding(req->encoding, __e);
}

__fly_unused static inline bool fly_is_accept_type(fly_encoding_t *e, fly_encoding_type_t *type)
{
	struct fly_bllist *__b;
	struct __fly_encoding *__a;

	fly_for_each_bllist(__b, &e->accepts){
		__a = fly_bllist_data(__b, struct __fly_encoding, blelem);
		if (__a->type == type)
			return true;
	}
	return false;
}

int fly_accept_encoding(struct fly_request *req)
{
	fly_hdr_ci *header;
	fly_hdr_c  *accept_encoding;

	header = req->header;

#ifdef DEBUG
	assert(req != NULL && req->pool != NULL && req->header != NULL);
#endif

	__fly_encode_init(req);
	switch (__fly_accept_encoding(header, &accept_encoding)){
	case __FLY_ACCEPT_ENCODING_ERROR:
		req->encoding = NULL;
		return FLY_ACCEPT_ENCODING_ERROR;
	case __FLY_ACCEPT_ENCODING_NOTFOUND:
		__fly_add_accept_encode_asterisk(req);
		switch(__fly_decide_encoding(req->encoding)){
		case __FLY_DECIDE_ENCODING_SUCCESS:
			return FLY_ACCEPT_ENCODING_SUCCESS;
		case __FLY_DECIDE_ENCODING_ERROR:
			return FLY_ACCEPT_ENCODING_ERROR;
		case __FLY_DECIDE_ENCODING_NOT_ACCEPTABLE:
			return FLY_ACCEPT_ENCODING_NOT_ACCEPTABLE;
		default:
			FLY_NOT_COME_HERE
		}
	case __FLY_ACCEPT_ENCODING_FOUND:
		switch(__fly_parse_accept_encoding(req, accept_encoding)){
		case __FLY_PARSE_ACCEPT_ENCODING_PARSEERROR:
			return FLY_ACCEPT_ENCODING_SYNTAX_ERROR;
		case __FLY_PARSE_ACCEPT_ENCODING_SUCCESS:
			return FLY_ACCEPT_ENCODING_SUCCESS;
		case __FLY_PARSE_ACCEPT_ENCODING_ERROR:
			return FLY_ACCEPT_ENCODING_ERROR;
		case __FLY_PARSE_ACCEPT_ENCODING_NOT_ACCEPTABLE:
			return FLY_ACCEPT_ENCODING_NOT_ACCEPTABLE;
		default:
			FLY_NOT_COME_HERE
		}
	default:
		FLY_NOT_COME_HERE
	}
	FLY_NOT_COME_HERE
	return -1;
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

				memset(encname, 0, FLY_ENCNAME_MAXLEN);
				__fly_memcpy_name(encname, name!=NULL ? name : "*", strlen(name!=NULL ? name : "*"), FLY_ENCNAME_MAXLEN);
				encname[FLY_ENCNAME_MAXLEN-1] = '\0';

				ne->type = fly_encoding_from_name(encname);
				ne->use  = false;
				ne->encoding = e;
				ne->quality_value = __fly_quality_value_from_str(qvalue);

				/* only add supported encoding */
				if (ne->type == NULL)
					goto end_of_add;

				__fly_add_accept_encoding(e, ne);
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
	default:
		FLY_NOT_COME_HERE
	}

	/* decide to use which encodes */
	switch(__fly_decide_encoding(req->encoding)){
	case __FLY_DECIDE_ENCODING_SUCCESS:
		return __FLY_PARSE_ACCEPT_ENCODING_SUCCESS;
	case __FLY_DECIDE_ENCODING_ERROR:
		return __FLY_PARSE_ACCEPT_ENCODING_ERROR;
	case __FLY_DECIDE_ENCODING_NOT_ACCEPTABLE:
		return __FLY_PARSE_ACCEPT_ENCODING_NOT_ACCEPTABLE;
	default:
		FLY_NOT_COME_HERE
	}
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
	if (__e == NULL || !__e->accept_count)
		return 0;

	int max_quality_value = 0;
	struct __fly_encoding *maxt = NULL, *accept;
	fly_encoding_type_t *__p;
	struct fly_bllist *__b;

	fly_for_each_bllist(__b, &__e->accepts){
		accept = fly_bllist_data(__b, struct __fly_encoding, blelem);
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

	/* If there is no encoding type that have quality value greater than 0, response 406(Not Acceptable)  */
	if (maxt == NULL)
		return __FLY_DECIDE_ENCODING_NOT_ACCEPTABLE;
	/* If asterisk, select most priority encoding. */
	if (maxt->type == __fly_asterisk()){
		__p = __fly_most_priority();
		maxt->type = __p;
		maxt->use = true;
	}
	return __FLY_DECIDE_ENCODING_SUCCESS;
}

fly_encname_t *fly_decided_encoding_name(fly_encoding_t *enc)
{
	if (enc->accept_count == 0)
		return NULL;

	struct fly_bllist *__b;
	struct __fly_encoding *__e;

	fly_for_each_bllist(__b, &enc->accepts){
		__e = fly_bllist_data(__b, struct __fly_encoding, blelem);
		if (__e->use)
			return __e->type->name;
	}
	return NULL;
}

fly_encoding_type_t *fly_decided_encoding_type(fly_encoding_t *enc)
{
	if (enc->accept_count == 0)
		return NULL;

	struct fly_bllist *__b;
	struct __fly_encoding *__e;

	fly_for_each_bllist(__b, &enc->accepts){
		__e = fly_bllist_data(__b, struct __fly_encoding, blelem);
		if (__e->use)
			return __e->type;
	}
	return NULL;
}

fly_buffer_c *fly_e_buf_add(fly_de_t *de)
{
	if (fly_buffer_add_chain(de->encbuf) == -1)
		return NULL;

	return fly_buffer_last_chain(de->encbuf);
}

fly_buffer_c *fly_d_buf_add(fly_de_t *de)
{
	if (fly_buffer_add_chain(de->decbuf) == -1)
		return NULL;

	return fly_buffer_last_chain(de->decbuf);
}

fly_encoding_type_t *fly_supported_content_encoding(fly_hdr_value *value)
{
	for (fly_encoding_type_t *__t=__fly_encodes; __t; __t++){
		if (strcmp(__t->name, value) == 0){
			return __t;
		}
	}
	return NULL;
}

struct fly_de *fly_de_init(fly_pool_t *pool)
{
	struct fly_de *de;
	de = fly_pballoc(pool, sizeof(struct fly_de));
	if (fly_unlikely_null(de))
		return NULL;

	de->pool = pool;
	de->encbuf = NULL;
	de->decbuf = NULL;
	de->offset = 0;
	de->count = 0;
	de->fd = -1;
	de->c_sockfd = -1;
	de->bfs = 0;
	de->event = NULL;
	de->response = NULL;
	de->contlen = 0;
	de->end = false;
	de->already_ptr = NULL;
	de->already_len = 0;
	de->target_already_alloc = false;
	de->overflow = false;

	return de;
}

void fly_de_release(fly_de_t *de)
{
	fly_pool_t *__pool;

	__pool = de->pool;
	fly_pbfree(__pool, de);
	return;
}

/* test whetheer type in enc */
bool fly_encoding_matching(struct fly_encoding *enc, struct fly_encoding_type *type)
{
	struct fly_bllist *__b;
	struct __fly_encoding *__e;
	fly_for_each_bllist(__b, &enc->accepts){
		__e = fly_bllist_data(__b, struct __fly_encoding, blelem);
		if (__e->type->type == type->type)
			return true;
	}
	/* not found */
	return false;
}

size_t fly_encode_threshold(void)
{
	return (size_t) fly_config_value_long(FLY_ENCODE_THRESHOLD);
}
