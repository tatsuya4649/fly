#include <ctype.h>
#include <string.h>
#include "encode.h"

fly_encoding_t encodes[] = {
	FLY_ENCODE_TYPE(gzip),
	FLY_ENCODE_TYPE(compress),
	FLY_ENCODE_TYPE(deflate),
	FLY_ENCODE_TYPE(identity),
	FLY_ENCODE_TYPE(br),

	FLY_ENCODE_NULL
};

fly_encoding_t *fly_encode_from_type(fly_encoding_e type)
{
	for (fly_encoding_t *e=encodes; FLY_ENCODE_END(e); e++){
		if (e->type == type)
			return e;
	}
	return NULL;
}

fly_encoding_t *fly_encode_from_name(fly_encname_t *name)
{
	#define FLY_ENCODE_NAME_LENGTH		20
	fly_encname_t enc_name[FLY_ENCODE_NAME_LENGTH];
	fly_encname_t *ptr;

	ptr = enc_name;
	for (; *name; name++)
		*ptr++ = tolower(*name);
	*ptr = '\0';

	for (fly_encoding_t *e=encodes; FLY_ENCODE_END(e); e++){
		if (strcmp(e->name, enc_name) == 0)
			return e;
	}
	return NULL;
	#undef FLY_ENCODE_NAME_LENGTH
}
