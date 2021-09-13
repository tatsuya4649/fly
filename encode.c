#include "encode.h"

__fly_static fly_encoding_type_t __fly_encodes[] = {
	FLY_ENCODE_TYPE(gzip),
	{ fly_gzip, "x-gzip" },
	FLY_ENCODE_TYPE(compress),
	{ fly_compress, "x-compress" },
	FLY_ENCODE_TYPE(deflate),
	FLY_ENCODE_TYPE(identity),
	FLY_ENCODE_TYPE(br),
	FLY_ENCODE_ASTERISK,
	FLY_ENCODE_NULL
};

fly_encname_t *fly_encname_from_type(fly_encoding_e type)
{
	for (fly_encoding_type_t *e=__fly_encodes; FLY_ENCODE_END(e); e++){
		if (e->type == type)
			return e->name;
	}
	return NULL;
}

fly_encoding_type_t *fly_encode_from_type(fly_encoding_e type)
{
	for (fly_encoding_type_t *e=__fly_encodes; FLY_ENCODE_END(e); e++){
		if (e->type == type)
			return e;
	}
	return NULL;
}

fly_encoding_type_t *fly_encode_from_name(fly_encname_t *name)
{
	#define FLY_ENCODE_NAME_LENGTH		20
	fly_encname_t enc_name[FLY_ENCODE_NAME_LENGTH];
	fly_encname_t *ptr;

	ptr = enc_name;
	for (; *name; name++)
		*ptr++ = tolower(*name);
	*ptr = '\0';

	for (fly_encoding_type_t *e=__fly_encodes; FLY_ENCODE_END(e); e++){
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

int fly_gzip_encode(fly_encbuf_t *encbuf, size_t encbuflen, fly_encbuf_t *decbuf, size_t decbuflen)
{
	int status, flush;
	z_stream __zstream;

	if (encbuf == NULL || !encbuflen || decbuf == NULL || !decbuflen)
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

int fly_identify_decode(fly_encbuf_t *encbuf, size_t encbuflen, fly_encbuf_t *decbuf, size_t decbuflen)
{
	memset(decbuf, '\0', decbuflen);
	memcpy(decbuf, encbuf, encbuflen);
	return 0;
}

int fly_identify_encode(fly_encbuf_t *encbuf, size_t encbuflen, fly_encbuf_t *decbuf, size_t decbuflen)
{
	memset(encbuf, '\0', encbuflen);
	memcpy(encbuf, decbuf, decbuflen);
	return 0;
}

#include "header.h"
int fly_accept_encoding(fly_request_t *req)
{
	__unused fly_hdr_ci *header;

	header = req->header;
	if (req == NULL || req->header == NULL)
		return -1;
	return 0;
}
