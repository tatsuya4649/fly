#include "mime.h"
#include "request.h"

fly_mime_type_t mimes[] = {
	{text_plain, "text/plain", FLY_STRING_ARRAY("txt", NULL)},
	{0, "", NULL}
};

__fly_static int __fly_mime_init(fly_request_t *req);
__fly_static int __fly_add_accept_mime(fly_mime_t *m, struct __fly_mime *nm);
__fly_static int __fly_accept_parse(fly_mime_t *mime, fly_hdr_c *c);
__fly_static int __fly_accept_mime_parse(fly_mime_t *mime, fly_hdr_value *value);
#define FLY_MIME_PARSE_ERROR	-1
#define FLY_MIME_PARSE_PERROR	0
#define FLY_MIME_PARSE_SUCCESS	1
__fly_static int __fly_accept_type_from_str(fly_mime_t *mime, struct __fly_mime_type *type, fly_hdr_value *type_str);
__fly_static int __fly_accept_subtype_from_str(struct __fly_mime_subtype *subtype, fly_hdr_value *subtype_str);
__fly_static int __fly_accept_qvalue_from_str(fly_hdr_value *qptr);

static inline bool __fly_digit(char c);
static inline bool __fly_lalpha(char c);
static inline bool __fly_q(char c);
static inline bool __fly_equal(char c);
static inline bool __fly_zero(char c);
static inline bool __fly_zeros(char c);
static inline bool __fly_one(char c);
static inline bool __fly_point(char c);

static inline bool __fly_ualpha(char c);
static inline bool __fly_comma(char c);
static inline bool __fly_alpha(char c);
static inline bool __fly_vchar(char c);
static inline bool __fly_tchar(char c);
static inline bool __fly_token(char c);
static inline bool __fly_type(char c);
static inline bool __fly_asterisk(char c);
static inline bool __fly_subtype(char c);
static inline bool __fly_slash(char c);
static inline bool __fly_semicolon(char c);
static inline bool __fly_quoted(char c);
static inline bool __fly_space(char c);
static inline bool __fly_qdtext(char c);
static inline bool __fly_obs_text(unsigned c);
static inline bool __fly_quoted_pair(char *c);
static inline bool __fly_quoted_string(char *c);


fly_mime_type_t *fly_mime_from_type(fly_mime_e type)
{
	for (fly_mime_type_t *m=mimes; m->extensions!=NULL; m++){
		if (m->type == type)
			return m;
	}
	return NULL;
}

__fly_static int __fly_mime_init(fly_request_t *req)
{
	fly_mime_t *mime;

	mime = fly_pballoc(req->pool, sizeof(fly_mime_t));
	if (mime == NULL)
		return -1;
	mime->acqty = 0;
	mime->accepts = NULL;
	mime->request = req;
	req->mime = mime;

	return 0;
}

__fly_static int __fly_accept_mime(fly_hdr_ci *header, fly_hdr_c **c)
{
#define __FLY_ACCEPT_MIME_NOTFOUND		0
#define __FLY_ACCEPT_MIME_FOUND			1
#define __FLY_ACCEPT_MIME_ERROR			-1
	fly_hdr_c *__h;

	if (header->chain_length==0)
		return __FLY_ACCEPT_MIME_NOTFOUND;

	for (__h=header->entry; __h; __h=__h->next){
		if (strcmp(__h->name, FLY_ACCEPT_HEADER) == 0 && __h->value){
			*c = __h;
			return __FLY_ACCEPT_MIME_FOUND;
		}
	}
	return __FLY_ACCEPT_MIME_NOTFOUND;
}

__fly_static int __fly_add_accept_mime(fly_mime_t *m, struct __fly_mime *nm)
{
	if (m->acqty == 0){
		m->accepts = nm;
		nm->next = NULL;
	}else{
		struct __fly_mime *__m, *prev;
		for (__m=m->accepts; __m->next; __m=__m->next){
			if (fly_same_type(__m, nm)){
				if (__m->quality_value > nm->quality_value){
					nm->next = __m->next;
					if (prev)
						prev->next = nm;
					/* TODO: release __m */
					goto increment;
				}else if (__m->quality_value == nm->quality_value){
					if (__m->extqty > nm->extqty){
						nm->next = __m->next;
						if (prev)
							prev->next = nm;
						/* TODO: release __m */
						goto increment;
					}
				}
			}
			prev = __m;
		}
		__m->next = nm;
	}
increment:
	m->acqty++;
	return 0;
}

__fly_static int __fly_add_accept_asterisk(fly_request_t *req)
{
	struct __fly_mime *am;

	am = fly_pballoc(req->pool, sizeof(struct __fly_mime));
	if (am == NULL)
		return -1;

	FLY_MIME_ASTERISK(am);
	return __fly_add_accept_mime(req->mime, am);
}

int fly_accept_mime(__unused fly_request_t *request)
{
	fly_hdr_ci *header;
	fly_hdr_c  *accept;

	header = request->header;
	if (request == NULL || request->pool == NULL || request->header == NULL)
		return -1;

	if (__fly_mime_init(request) == -1)
		return -1;

	switch(__fly_accept_mime(header, &accept)){
	case __FLY_ACCEPT_MIME_ERROR:
		return -1;
	case __FLY_ACCEPT_MIME_NOTFOUND:
		if (__fly_add_accept_asterisk(request) == -1)
			return -1;
		return 0;
	case __FLY_ACCEPT_MIME_FOUND:
		return __fly_accept_parse(request->mime, accept);
	default:
		return -1;
	}
	FLY_NOT_COME_HERE
}

static inline bool __fly_zero(char c)
{
	return (c == 0x30) ? true : false;
}

static inline bool __fly_zeros(char c)
{
	return (c == '\0') ? true : false;
}

static inline bool __fly_one(char c)
{
	return (c == 0x31) ? true : false;
}

static inline bool __fly_equal(char c)
{
	return (c == 0x3D) ? true : false;
}

static inline bool __fly_point(char c)
{
	return (c == 0x2E) ? true : false;
}

static inline bool __fly_digit(char c)
{
	return (c>=0x30 && c<=0x39) ? true : false;
}

static inline bool __fly_lalpha(char c)
{
	return (c>=0x61 && c<=0x7A) ? true : false;
}

static inline bool __fly_ualpha(char c)
{
	return (c>=0x41 && c<=0x5A) ? true : false;
}

static inline bool __fly_alpha(char c)
{
	return (__fly_lalpha(c) || __fly_ualpha(c)) ? true : false;
}

static inline bool __fly_vchar(char c)
{
	return (c>=0x21 && c<=0x7E) ? true : false;
}

static inline bool __fly_q(char c)
{
	return (c=='q') ? true : false;
}

static inline bool __fly_tchar(char c)
{
	return ( \
		__fly_alpha(c) || __fly_digit(c) || (__fly_vchar(c) && c != ';') || \
		c=='!' || c=='#' || c=='$' || c=='%' || c=='&' || c=='\'' || c=='*' || \
		c=='+' || c=='-' || c=='.' || c=='^' || c=='_' || c=='`' || c== '|' || \
		c=='~' \
	) ? true : false;
}

static inline bool __fly_token(char c)
{
	return __fly_tchar(c) ? true : false;
}

static inline bool __fly_type(char c)
{
	return __fly_token(c) ? true : false;
}

static inline bool __fly_asterisk(char c)
{
	return (c=='*') ? true : false;
}

static inline bool __fly_subtype(char c)
{
	return __fly_token(c) ? true : false;
}

static inline bool __fly_slash(char c)
{
	return (c == '/') ? true : false;
}

static inline bool __fly_semicolon(char c)
{
	return (c == ';') ? true : false;
}

static inline bool __fly_quoted(char c)
{
	return (c == 0x22) ? true : false;
}

static inline bool __fly_space(char c)
{
	return (c == 0x20 || c == 0x9) ? true : false;
}

static inline bool __fly_comma(char c)
{
	return (c == 0x2C) ? true : false;
}

static inline bool __fly_qdtext(char c)
{
	return (	\
		__fly_space(c) || c==0x21 || (c>=0x23&&c<=0x5B) ||  \
		(c>=0x5D&&c<=0x7E) || __fly_obs_text(c)				\
	)? true : false;
}

static inline bool __fly_obs_text(unsigned c)
{
	return (c>=0x80 && c<=0xFF) ? true : false;
}

static inline bool __fly_quoted_pair(char *c)
{
	return ((*c == 0x5C) && (__fly_space(*(c+1)) || \
	__fly_vchar(*(c+1)) || __fly_obs_text(*(c+1))) \
	) ? true : false;
}

static inline bool __fly_quoted_string(char *c)
{
	return (__fly_qdtext(*c) || __fly_quoted_pair(c)) ? true : false;
}

__fly_static int __fly_accept_mime_parse(__unused fly_mime_t *mime, fly_hdr_value *value)
{
	fly_hdr_value *ptr;
	__unused char *param_tokenl = NULL, *param_tokenr=NULL, *qvalue_ptr=NULL, *ext_tokenl=NULL, *ext_tokenr=NULL, *type=NULL, *subtype=NULL;
	int decimal_places;
	struct __fly_mime *__nm;

	__unused enum{
		MEDIA_RANGE,
		ACCEPT_PARAMS,
	} pfase;
	__unused enum{
		__FLY_ACCEPT_MIME_MEDIA_RANGE_INIT,
		__FLY_ACCEPT_MIME_MEDIA_RANGE_TYPE,
		__FLY_ACCEPT_MIME_MEDIA_RANGE_SLASH,
		__FLY_ACCEPT_MIME_MEDIA_RANGE_SUBTYPE,
		__FLY_ACCEPT_MIME_MEDIA_RANGE_OWS1,
		__FLY_ACCEPT_MIME_MEDIA_RANGE_SEMICOLON,
		__FLY_ACCEPT_MIME_MEDIA_RANGE_MAYBEQ,
		__FLY_ACCEPT_MIME_MEDIA_RANGE_OWS2,
		__FLY_ACCEPT_MIME_MEDIA_RANGE_PARAMETER_TOKENL,
		__FLY_ACCEPT_MIME_MEDIA_RANGE_PARAMETER_EQUAL,
		__FLY_ACCEPT_MIME_MEDIA_RANGE_PARAMETER_TOKENR,
		__FLY_ACCEPT_MIME_MEDIA_RANGE_PARAMETER_QUOTED_STRINGL,
		__FLY_ACCEPT_MIME_MEDIA_RANGE_PARAMETER_QUOTED_STRING,
		__FLY_ACCEPT_MIME_MEDIA_RANGE_PARAMETER_QUOTED_STRINGR,
		__FLY_ACCEPT_MIME_ACCEPT_PARAMS_WEIGHT_Q,
		__FLY_ACCEPT_MIME_ACCEPT_PARAMS_WEIGHT_OWS2,
		__FLY_ACCEPT_MIME_ACCEPT_PARAMS_WEIGHT_EQUAL,
		__FLY_ACCEPT_MIME_ACCEPT_PARAMS_WEIGHT_ONE_INT,
		__FLY_ACCEPT_MIME_ACCEPT_PARAMS_WEIGHT_ONE_POINT,
		__FLY_ACCEPT_MIME_ACCEPT_PARAMS_WEIGHT_ONE_DECIMAL_PLACE,
		__FLY_ACCEPT_MIME_ACCEPT_PARAMS_WEIGHT_ZERO_INT,
		__FLY_ACCEPT_MIME_ACCEPT_PARAMS_WEIGHT_ZERO_POINT,
		__FLY_ACCEPT_MIME_ACCEPT_PARAMS_WEIGHT_ZERO_DECIMAL_PLACE,
		__FLY_ACCEPT_MIME_ACCEPT_PARAMS_EXT_OWS1,
		__FLY_ACCEPT_MIME_ACCEPT_PARAMS_EXT_SEMICOLON,
		__FLY_ACCEPT_MIME_ACCEPT_PARAMS_EXT_OWS2,
		__FLY_ACCEPT_MIME_ACCEPT_PARAMS_EXT_TOKENL,
		__FLY_ACCEPT_MIME_ACCEPT_PARAMS_EXT_EQUAL,
		__FLY_ACCEPT_MIME_ACCEPT_PARAMS_EXT_TOKENR,
		__FLY_ACCEPT_MIME_ACCEPT_PARAMS_EXT_QUOTED_STRINGL,
		__FLY_ACCEPT_MIME_ACCEPT_PARAMS_EXT_QUOTED_STRING,
		__FLY_ACCEPT_MIME_ACCEPT_PARAMS_EXT_QUOTED_STRINGR,
		__FLY_ACCEPT_MIME_ACCEPT_PARAMS_EXT_TOKENEND,
		__FLY_ACCEPT_MIME_COMMA,
		__FLY_ACCEPT_MIME_ADD,
		__FLY_ACCEPT_MIME_COMMA_AFTER_ADD,
		__FLY_ACCEPT_MIME_ACCEPT_PARAMS_END,
	} pstatus;

	ptr = value;
	pfase = MEDIA_RANGE;
	pstatus = __FLY_ACCEPT_MIME_MEDIA_RANGE_INIT;
	while(true){
		switch(pstatus){
		case __FLY_ACCEPT_MIME_MEDIA_RANGE_INIT:
			decimal_places = 0;
			qvalue_ptr = NULL;
			param_tokenl = NULL;
			param_tokenr = NULL;
			ext_tokenl = NULL;
			ext_tokenr = NULL;
			type = NULL;
			subtype = NULL;
			__nm = NULL;
			__nm = fly_pballoc(mime->request->pool, sizeof(struct __fly_mime));
			if (__nm == NULL)
				return FLY_MIME_PARSE_ERROR;

			if (__fly_type(*ptr) || __fly_asterisk(*ptr)){
				type = ptr;
				pstatus = __FLY_ACCEPT_MIME_MEDIA_RANGE_TYPE;
				continue;
			}

			return FLY_MIME_PARSE_PERROR;
		case __FLY_ACCEPT_MIME_MEDIA_RANGE_TYPE:
			if (__fly_slash(*ptr)){
				pstatus = __FLY_ACCEPT_MIME_MEDIA_RANGE_SLASH;
				break;
			}
			if (__fly_type(*ptr) || __fly_asterisk(*ptr))	break;

			goto perror;
		case __FLY_ACCEPT_MIME_MEDIA_RANGE_SLASH:
			if (__fly_subtype(*ptr) || __fly_asterisk(*ptr)){
				subtype = ptr;
				pstatus = __FLY_ACCEPT_MIME_MEDIA_RANGE_SUBTYPE;
				continue;
			}

			goto perror;
		case __FLY_ACCEPT_MIME_MEDIA_RANGE_SUBTYPE:
			if (__fly_comma(*ptr)){
				pstatus = __FLY_ACCEPT_MIME_COMMA;
				continue;
			}
			if (__fly_zeros(*ptr)){
				pstatus = __FLY_ACCEPT_MIME_ACCEPT_PARAMS_END;
				continue;
			}

			if (__fly_subtype(*ptr) || __fly_asterisk(*ptr)) break;

			if (__fly_space(*ptr)){
				pstatus = __FLY_ACCEPT_MIME_MEDIA_RANGE_OWS1;
				continue;
			}

			if (__fly_semicolon(*ptr)){
				pstatus = __FLY_ACCEPT_MIME_MEDIA_RANGE_SEMICOLON;
				break;
			}

			goto perror;
		case __FLY_ACCEPT_MIME_MEDIA_RANGE_OWS1:
			if (__fly_space(*ptr))
				break;

			if (__fly_semicolon(*ptr)){
				pstatus = __FLY_ACCEPT_MIME_MEDIA_RANGE_SEMICOLON;
				continue;
			}

			goto perror;
		case __FLY_ACCEPT_MIME_MEDIA_RANGE_SEMICOLON:
			if (__fly_space(*ptr)){
				pstatus = __FLY_ACCEPT_MIME_MEDIA_RANGE_OWS2;
				continue;
			}

			if (__fly_q(*ptr)){
				param_tokenl = ptr;
				pstatus = __FLY_ACCEPT_MIME_MEDIA_RANGE_MAYBEQ;
				break;
			}

			if (__fly_token(*ptr)){
				param_tokenl = ptr;
				pstatus = __FLY_ACCEPT_MIME_MEDIA_RANGE_PARAMETER_TOKENL;
				continue;
			}

			goto perror;
		case __FLY_ACCEPT_MIME_MEDIA_RANGE_MAYBEQ:
			if (__fly_equal(*ptr)){
				param_tokenl = NULL;
				pstatus = __FLY_ACCEPT_MIME_ACCEPT_PARAMS_WEIGHT_EQUAL;
				break;
			}

			pstatus = __FLY_ACCEPT_MIME_MEDIA_RANGE_PARAMETER_TOKENL;
			continue;
		case __FLY_ACCEPT_MIME_MEDIA_RANGE_OWS2:
			if (__fly_space(*ptr))	break;

			if (__fly_q(*ptr)){
				param_tokenl = ptr;
				pstatus = __FLY_ACCEPT_MIME_MEDIA_RANGE_MAYBEQ;
				break;
			}

			if (__fly_token(*ptr)){
				param_tokenl = ptr;
				pstatus = __FLY_ACCEPT_MIME_MEDIA_RANGE_PARAMETER_TOKENL;
				continue;
			}

			goto perror;
		case __FLY_ACCEPT_MIME_MEDIA_RANGE_PARAMETER_TOKENL:
			if (__fly_equal(*ptr)){
				pstatus = __FLY_ACCEPT_MIME_MEDIA_RANGE_PARAMETER_EQUAL;
				break;
			}
			if (__fly_token(*ptr))	break;

			goto perror;
		case __FLY_ACCEPT_MIME_MEDIA_RANGE_PARAMETER_EQUAL:
			if (__fly_quoted(*ptr)){
				pstatus = __FLY_ACCEPT_MIME_MEDIA_RANGE_PARAMETER_QUOTED_STRINGL;
				continue;
			}
			if (__fly_token(*ptr)){
				param_tokenr = ptr;
				pstatus = __FLY_ACCEPT_MIME_MEDIA_RANGE_PARAMETER_TOKENR;
				continue;
			}

			goto perror;
		case __FLY_ACCEPT_MIME_MEDIA_RANGE_PARAMETER_TOKENR:
			if (__fly_token(*ptr))
				break;

			/* TODO: Add params */
			if (__fly_zeros(*ptr)){
				pstatus = __FLY_ACCEPT_MIME_ACCEPT_PARAMS_END;
				continue;
			}
			if (__fly_space(*ptr)){
				pstatus = __FLY_ACCEPT_MIME_MEDIA_RANGE_OWS1;
				continue;
			}
			if (__fly_semicolon(*ptr)){
				pstatus = __FLY_ACCEPT_MIME_MEDIA_RANGE_SEMICOLON;
				break;
			}

			goto perror;
		case __FLY_ACCEPT_MIME_MEDIA_RANGE_PARAMETER_QUOTED_STRINGL:
			if (__fly_quoted(*ptr))
				break;

			if (__fly_quoted_string(ptr)){
				pstatus = __FLY_ACCEPT_MIME_MEDIA_RANGE_PARAMETER_QUOTED_STRING;
				continue;
			}

			goto perror;
		case __FLY_ACCEPT_MIME_MEDIA_RANGE_PARAMETER_QUOTED_STRING:
			if (__fly_quoted_string(ptr))	break;

			if (__fly_quoted(*ptr)){
				pstatus = __FLY_ACCEPT_MIME_MEDIA_RANGE_PARAMETER_QUOTED_STRINGR;
				break;
			}

			goto perror;
		case __FLY_ACCEPT_MIME_MEDIA_RANGE_PARAMETER_QUOTED_STRINGR:
			pstatus = __FLY_ACCEPT_MIME_MEDIA_RANGE_OWS1;
			continue;
		case __FLY_ACCEPT_MIME_ACCEPT_PARAMS_WEIGHT_Q:
			pfase = ACCEPT_PARAMS;
			if (__fly_q(*ptr))
				break;
			if (__fly_equal(*ptr)){
				pstatus = __FLY_ACCEPT_MIME_ACCEPT_PARAMS_WEIGHT_EQUAL;
				break;
			}

			goto perror;
		case __FLY_ACCEPT_MIME_ACCEPT_PARAMS_WEIGHT_EQUAL:
			if (__fly_one(*ptr)){
				qvalue_ptr = ptr;
				pstatus = __FLY_ACCEPT_MIME_ACCEPT_PARAMS_WEIGHT_ONE_INT;
				break;
			}
			if (__fly_zero(*ptr)){
				qvalue_ptr = ptr;
				pstatus = __FLY_ACCEPT_MIME_ACCEPT_PARAMS_WEIGHT_ZERO_INT;
				break;
			}

			goto perror;
		case __FLY_ACCEPT_MIME_ACCEPT_PARAMS_WEIGHT_ONE_INT:
			if (__fly_point(*ptr)){
				pstatus = __FLY_ACCEPT_MIME_ACCEPT_PARAMS_WEIGHT_ONE_POINT;
				break;
			}

			goto perror;
		case __FLY_ACCEPT_MIME_ACCEPT_PARAMS_WEIGHT_ONE_POINT:
			if (__fly_one(*ptr)){
				pstatus = __FLY_ACCEPT_MIME_ACCEPT_PARAMS_WEIGHT_ONE_DECIMAL_PLACE;
				continue;
			}

			goto perror;
		case __FLY_ACCEPT_MIME_ACCEPT_PARAMS_WEIGHT_ONE_DECIMAL_PLACE:
			if (__fly_zero(*ptr) && decimal_places++ < 3)	break;
			if (__fly_space(*ptr)){
				pstatus = __FLY_ACCEPT_MIME_ACCEPT_PARAMS_EXT_OWS1;
				continue;
			}
			if (__fly_zeros(*ptr)){
				pstatus = __FLY_ACCEPT_MIME_ACCEPT_PARAMS_END;
				continue;
			}
			goto perror;
		case __FLY_ACCEPT_MIME_ACCEPT_PARAMS_WEIGHT_ZERO_INT:
			if (__fly_point(*ptr)){
				pstatus = __FLY_ACCEPT_MIME_ACCEPT_PARAMS_WEIGHT_ZERO_POINT;
				break;
			}
			goto perror;
		case __FLY_ACCEPT_MIME_ACCEPT_PARAMS_WEIGHT_ZERO_POINT:
			if (__fly_digit(*ptr)){
				pstatus = __FLY_ACCEPT_MIME_ACCEPT_PARAMS_WEIGHT_ZERO_DECIMAL_PLACE;
				continue;
			}
			goto perror;
		case __FLY_ACCEPT_MIME_ACCEPT_PARAMS_WEIGHT_ZERO_DECIMAL_PLACE:
			if (__fly_digit(*ptr) && decimal_places++ < 3)	break;
			if (__fly_space(*ptr)){
				pstatus = __FLY_ACCEPT_MIME_ACCEPT_PARAMS_EXT_OWS1;
				continue;
			}
			if (__fly_semicolon(*ptr)){
				pstatus = __FLY_ACCEPT_MIME_ACCEPT_PARAMS_EXT_SEMICOLON;
				break;
			}
			if (__fly_comma(*ptr)){
				pstatus = __FLY_ACCEPT_MIME_COMMA;
				continue;
			}


			if (__fly_zeros(*ptr)){
				pstatus = __FLY_ACCEPT_MIME_ACCEPT_PARAMS_END;
				continue;
			}

			goto perror;
		case __FLY_ACCEPT_MIME_ACCEPT_PARAMS_EXT_OWS1:
			if (__fly_space(*ptr))	break;
			if (__fly_semicolon(*ptr)){
				pstatus = __FLY_ACCEPT_MIME_ACCEPT_PARAMS_EXT_SEMICOLON;
				break;
			}

			goto perror;
		case __FLY_ACCEPT_MIME_ACCEPT_PARAMS_EXT_SEMICOLON:
			if (__fly_space(*ptr)){
				pstatus = __FLY_ACCEPT_MIME_ACCEPT_PARAMS_EXT_OWS2;
				break;
			}

			if (__fly_token(*ptr)){
				pstatus = __FLY_ACCEPT_MIME_ACCEPT_PARAMS_EXT_TOKENL;
				continue;
			}
			goto perror;
		case __FLY_ACCEPT_MIME_ACCEPT_PARAMS_EXT_OWS2:
			if (__fly_space(*ptr))	break;

			if (__fly_token(*ptr)){
				ext_tokenl = ptr;
				pstatus = __FLY_ACCEPT_MIME_ACCEPT_PARAMS_EXT_TOKENL;
				continue;
			}
			goto perror;
		case __FLY_ACCEPT_MIME_ACCEPT_PARAMS_EXT_TOKENL:
			if (__fly_equal(*ptr)){
				pstatus = __FLY_ACCEPT_MIME_ACCEPT_PARAMS_EXT_EQUAL;
				break;
			}
			if (__fly_token(*ptr))	break;

			if (__fly_space(*ptr)){
				pstatus = __FLY_ACCEPT_MIME_ACCEPT_PARAMS_EXT_TOKENEND;
				continue;
			}
			goto perror;
		case __FLY_ACCEPT_MIME_ACCEPT_PARAMS_EXT_EQUAL:
			if (__fly_quoted(*ptr)){
				pstatus = __FLY_ACCEPT_MIME_ACCEPT_PARAMS_EXT_QUOTED_STRINGL;
				break;
			}
			if (__fly_token(*ptr)){
				ext_tokenr = ptr;
				pstatus = __FLY_ACCEPT_MIME_ACCEPT_PARAMS_EXT_TOKENR;
				continue;
			}
			goto perror;
		case __FLY_ACCEPT_MIME_ACCEPT_PARAMS_EXT_TOKENR:
			if (__fly_comma(*ptr)){
				pstatus = __FLY_ACCEPT_MIME_ACCEPT_PARAMS_EXT_TOKENEND;
				continue;
			}
			if (__fly_token(*ptr))	break;

			pstatus = __FLY_ACCEPT_MIME_ACCEPT_PARAMS_EXT_TOKENEND;
			continue;
		case __FLY_ACCEPT_MIME_ACCEPT_PARAMS_EXT_QUOTED_STRINGL:
			ext_tokenl=ptr;
			if (__fly_quoted_string(ptr)){
				pstatus = __FLY_ACCEPT_MIME_ACCEPT_PARAMS_EXT_QUOTED_STRING;
				continue;
			}

			goto perror;
		case __FLY_ACCEPT_MIME_ACCEPT_PARAMS_EXT_QUOTED_STRING:
			if (__fly_quoted(*ptr)){
				pstatus = __FLY_ACCEPT_MIME_ACCEPT_PARAMS_EXT_QUOTED_STRINGR;
				break;
			}

			if (__fly_quoted_string(ptr))	break;

			goto perror;
		case __FLY_ACCEPT_MIME_ACCEPT_PARAMS_EXT_QUOTED_STRINGR:
			pstatus = __FLY_ACCEPT_MIME_ACCEPT_PARAMS_EXT_TOKENEND;
			continue;

		case __FLY_ACCEPT_MIME_ACCEPT_PARAMS_EXT_TOKENEND:
			/* TODO: add extension attribute */

			if (__fly_comma(*ptr)){
				pstatus = __FLY_ACCEPT_MIME_COMMA;
				continue;
			}
			pstatus = __FLY_ACCEPT_MIME_ACCEPT_PARAMS_EXT_OWS1;
			continue;
		case __FLY_ACCEPT_MIME_COMMA:
			pstatus = __FLY_ACCEPT_MIME_ADD;
			continue;
		case __FLY_ACCEPT_MIME_ADD:
			/* TODO: add mime */
			{
				__nm->quality_value = __fly_accept_qvalue_from_str(qvalue_ptr);
				if (__nm->quality_value == -1)
					return FLY_MIME_PARSE_ERROR;

				__fly_accept_type_from_str(mime, &__nm->type, type);
				if(__fly_accept_subtype_from_str(&__nm->subtype, subtype) == -1)
					return FLY_MIME_PARSE_PERROR;
				__nm->next = NULL;

				__fly_add_accept_mime(mime, __nm);
			}
			pstatus = __FLY_ACCEPT_MIME_COMMA_AFTER_ADD;
			continue;
		case __FLY_ACCEPT_MIME_COMMA_AFTER_ADD:
			if (__fly_zeros(*ptr)){
				return FLY_MIME_PARSE_SUCCESS;
			}else if (__fly_space(*ptr))
				break;
			else if (__fly_comma(*ptr))
				break;

			pstatus = __FLY_ACCEPT_MIME_MEDIA_RANGE_INIT;
			continue;
		case __FLY_ACCEPT_MIME_ACCEPT_PARAMS_END:
			pstatus = __FLY_ACCEPT_MIME_ADD;
			continue;
		default:
			/* unknown status */
			return FLY_MIME_PARSE_ERROR;
		}

		ptr++;
	}

goto perror:
	/* TODO: release __nm */
	return FLY_MIME_PARSE_PERROR;

	return FLY_MIME_PARSE_SUCCESS;
}

static struct __fly_mime_type __fly_mime_type_list[] = {
	__FLY_MIME_TYPE(text),
	__FLY_MIME_TYPE(image),
	__FLY_MIME_TYPE(application),
	__FLY_MIME_TYPE(asterisk),
	__FLY_MIME_TYPE(unknown),
	__FLY_MIME_NULL
};

__fly_static int __fly_same_type(char *t1, char *t2)
{
	while(!__fly_slash(*t2))
		if (*t1++ != *t2++)
			return -1;

	return 0;
}

__fly_static int __fly_copy_type(char *dist, char *src)
{
	int i=0;
	while(!__fly_slash(*src)){
		if (i++ >= FLY_MIME_TYPE_MAXLEN)
			return -1;

		*dist++ = *src++;
	}

	return 0;
}

__fly_static int __fly_copy_subtype(char *dist, char *src)
{
	int i=0;
	while(__fly_subtype(*src)){
		if (i++ >= FLY_MIME_SUBTYPE_MAXLEN)
			return -1;

		*dist++ = *src++;
	}
	*dist = '\0';

	return 0;
}

__fly_static int __fly_accept_type_from_str(fly_mime_t *mime, struct __fly_mime_type *type, fly_hdr_value *type_str)
{
	for (struct __fly_mime_type *__t=__fly_mime_type_list; __t->type_name; __t++){
		if (__fly_same_type(__t->type_name, type_str) == 0){
			type->type = __t->type;
			type->type_name = __t->type_name;
			return 0;
		}
	}

	type->type = fly_mime_type_unknown;
	type->type_name = fly_pballoc(mime->request->pool, sizeof(char)*FLY_MIME_TYPE_MAXLEN);
	return __fly_copy_type(type->type_name, type_str);
}


__fly_static int __fly_accept_subtype_from_str(__unused struct __fly_mime_subtype *subtype, __unused fly_hdr_value *subtype_str)
{
	if (__fly_copy_subtype(subtype->subtype, subtype_str) == -1)
		return -1;

	if (strcmp(subtype->subtype, "*") == 0)
		subtype->asterisk = true;
	return 0;
}

__fly_static int __fly_accept_qvalue_from_str(fly_hdr_value *qvalue)
{
	char qv_str[FLY_MIMQVALUE_MAXLEN];
	char *qptr;
	double quality_value;

	if (qvalue == NULL || __fly_one(*qvalue))
		return 100;

	qptr = qv_str;
	while(__fly_zero(*qvalue) || __fly_point(*qvalue) || __fly_digit(*qvalue))
		*qptr++ = *qvalue++;
	*qptr = '\0';

	quality_value = atof(qv_str);

	if (quality_value < 0.0 || quality_value > 1.0)
		return -1;

	return (int) (quality_value*100);
}

__fly_static int __fly_accept_parse(fly_mime_t *mime, fly_hdr_c *c)
{
	fly_hdr_value *value;

	if (!mime)
		return -1;

	if (!c->value)
		return FLY_MIME_PARSE_ERROR;
	value = c->value;

	switch (__fly_accept_mime_parse(mime, value)){
	case FLY_MIME_PARSE_SUCCESS:
		return 1;
	case FLY_MIME_PARSE_ERROR:
		return -1;
	case FLY_MIME_PARSE_PERROR:
		return 0;
	default:
		return -1;
	}
	FLY_NOT_COME_HERE
}
