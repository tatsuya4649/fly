#include "lang.h"
#include "request.h"
#include "header.h"
#include "char.h"

static void __fly_accept_lang_init(fly_request_t *req);
static void __fly_accept_lang_add_asterisk(fly_request_t *req);
__fly_static void __fly_accept_lang_add(fly_lang_t *l, struct __fly_lang *__nc);
static inline bool __fly_vchar(char c);
static inline bool __fly_tchar(char c);
static inline bool __fly_token(char c);
static inline bool __fly_q(char c);
static inline bool __fly_one(char c);
static inline bool __fly_zeros(char c);
static inline bool __fly_zero(char c);

__fly_static int __fly_al_parse(fly_lang_t *l, fly_hdr_value *accept_lang);
__fly_static int __fly_accept_lang_parse(fly_request_t *req, fly_hdr_value *accept_lang);
static float __fly_qvalue_from_str(char *qvalue);
__fly_static void __fly_lname_cpy(char *dist, char *src);
static inline void __fly_check_of_asterisk(struct __fly_lang *__nc);
#define __FLY_ACCEPT_LANG_FOUND			(1)
#define __FLY_ACCEPT_LANG_NOTFOUND		(0)
#define __FLY_ACCEPT_LANG_ERROR			(-1)
__fly_static int __fly_accept_lang(fly_hdr_ci *header, fly_hdr_value **value);

__fly_static int __fly_accept_lang(fly_hdr_ci *header, fly_hdr_value **value)
{
	fly_hdr_c *__h;

	if (header->chain_count == 0)
		return __FLY_ACCEPT_LANG_NOTFOUND;

	struct fly_bllist *__b;
	fly_for_each_bllist(__b, &header->chain){
		__h = fly_bllist_data(__b, fly_hdr_c, blelem);
		if (__h->name_len>0 && strcmp(__h->name, FLY_ACCEPT_LANG) == 0){
			*value = __h->value;
			return __FLY_ACCEPT_LANG_FOUND;
		}
	}

	return __FLY_ACCEPT_LANG_NOTFOUND;
}

__fly_static void __fly_accept_lang_add(fly_lang_t *l, struct __fly_lang *__nl)
{
	fly_bllist_add_tail(&l->langs, &__nl->blelem);
	l->lang_count++;
}

static void __fly_accept_lang_add_asterisk(fly_request_t *req)
{
	struct __fly_lang *__l;

	__l = fly_pballoc(req->pool, sizeof(struct __fly_lang));
	strcpy(__l->lname, "*");
	__l->quality_value = FLY_LANG_QVALUE_MAX;
	__l->asterisk = true;
	__fly_accept_lang_add(req->language, __l);
}

static void __fly_accept_lang_init(fly_request_t *req)
{
	fly_lang_t *lang;
	fly_pool_t *pool;

	pool = req->pool;
	lang = fly_pballoc(pool, sizeof(fly_lang_t));
	lang->lang_count = 0;
	fly_bllist_init(&lang->langs);
	lang->request = req;
	req->language = lang;
}

static inline bool __fly_vchar(char c)
{
	return (c>=0x21 && c<=0x7E) ? true : false;
}
static inline bool __fly_tchar(char c)
{
	return (
		fly_alpha(c) || fly_numeral(c) || (__fly_vchar(c) && c != ';') || \
		c=='!' || c=='#' || c=='$' || c=='&' || c==0x27 || c=='*' || \
		c=='+' || c=='-' || c=='.' || c=='^' || c=='_' || c=='`' || \
		c=='|' || c=='~' \
	) ? true : false;
}
static inline bool __fly_token(char c)
{
	return __fly_tchar(c) ? true : false;
}

static inline bool __fly_lang(char c)
{
	return __fly_token(c) ? true : false;
}

static inline bool __fly_q(char c)
{
	return (c == 'q') ? true : false;
}

static inline bool __fly_semicolon(char c)
{
	return (c == ';') ? true : false;
}

static inline bool __fly_space(char c)
{
	return (c == 0x20 || c == 0x09) ? true : false;
}

static inline bool __fly_zeros(char c)
{
	return (c == '\0') ? true : false;
}

static inline bool __fly_zero(char c)
{
	return (c == '0') ? true : false;
}

static inline bool __fly_one(char c)
{
	return (c == '1') ? true : false;
}

static inline bool __fly_equal(char c)
{
	return (c == 0x3D) ? true : false;
}

static inline bool __fly_point(char c)
{
	return (c == 0x2E) ? true : false;
}

static inline bool __fly_comma(char c)
{
	return (c == 0x2C) ? true : false;
}

__fly_static int __fly_al_parse(fly_lang_t *l, fly_hdr_value *accept_lang)
{
#define __FLY_AL_PARSE_SUCCESS		1
#define __FLY_AL_PARSE_PERROR		0
#define __FLY_AL_PARSE_ERROR		-1
#define __FLY_AL_PARSE_PBSTATUS(s)		\
	{									\
		pstatus = (s);					\
		break;							\
	}
#define __FLY_AL_PARSE_PCSTATUS(s)		\
	{									\
		pstatus = (s);					\
		continue;							\
	}
	fly_hdr_value *ptr;
	enum{
		INIT,
		LANG,
		WOWS1,
		WSEMICOLON,
		WOWS2,
		WQ,
		WQEQUAL,
		WQ_ONE_INT,
		WQ_ONE_POINT,
		WQ_ONE_DECIMAL_PLACE,
		WQ_ZERO_INT,
		WQ_ZERO_POINT,
		WQ_ZERO_DECIMAL_PLACE,
		WEIGHT,
		COMMA,
		ADD,
		ADD_END,
		END,
	} pstatus;
	char *lang_str=NULL, *qvalue=NULL;
	int decimal_places=0;

	ptr = accept_lang;
	pstatus = INIT;
	while(true)
	{
		switch(pstatus){
		case INIT:
			lang_str = NULL;
			qvalue = NULL;
			decimal_places = 0;
			if (__fly_lang(*ptr)){
				lang_str = ptr;
				__FLY_AL_PARSE_PCSTATUS(LANG);
			}

			goto perror;
		case LANG:
			if (__fly_comma(*ptr))
				__FLY_AL_PARSE_PCSTATUS(COMMA);
			if (__fly_zeros(*ptr))
				__FLY_AL_PARSE_PCSTATUS(ADD);
			if (__fly_semicolon(*ptr))
				__FLY_AL_PARSE_PBSTATUS(WSEMICOLON);
			if (__fly_space(*ptr))
				__FLY_AL_PARSE_PCSTATUS(WOWS1);

			if (__fly_lang(*ptr)) break;
			goto perror;
		case WOWS1:
			if (__fly_space(*ptr))	break;
			if (__fly_semicolon(*ptr))
					__FLY_AL_PARSE_PBSTATUS(WSEMICOLON);
			goto perror;
		case WSEMICOLON:
			if (__fly_space(*ptr))
				__FLY_AL_PARSE_PBSTATUS(WOWS2);
			if (__fly_q(*ptr))
				__FLY_AL_PARSE_PBSTATUS(WQ);

			goto perror;
		case WOWS2:
			if (__fly_space(*ptr))	break;
			if (__fly_q(*ptr))
				__FLY_AL_PARSE_PBSTATUS(WQ);

			goto perror;
		case WQ:
			if (__fly_equal(*ptr))
				__FLY_AL_PARSE_PBSTATUS(WQEQUAL);

			goto perror;
		case WQEQUAL:
			if (__fly_one(*ptr)){
				/* start of quality value */
				qvalue = ptr;
				__FLY_AL_PARSE_PBSTATUS(WQ_ONE_INT);
			}
			if (__fly_zero(*ptr)){
				/* start of quality value */
				qvalue = ptr;
				__FLY_AL_PARSE_PBSTATUS(WQ_ZERO_INT);
			}

			goto perror;
		case WQ_ONE_INT:
			if (__fly_point(*ptr))
				__FLY_AL_PARSE_PBSTATUS(WQ_ONE_POINT);
			if (__fly_comma(*ptr))
				__FLY_AL_PARSE_PCSTATUS(COMMA);
			if (__fly_zeros(*ptr))
				__FLY_AL_PARSE_PCSTATUS(ADD);
			goto perror;
		case WQ_ONE_POINT:
			if (__fly_zero(*ptr))
				__FLY_AL_PARSE_PCSTATUS(WQ_ONE_DECIMAL_PLACE);
			goto perror;
		case WQ_ONE_DECIMAL_PLACE:
			if (__fly_zero(*ptr) && decimal_places++ < 3)	break;
			if (__fly_comma(*ptr))
				__FLY_AL_PARSE_PCSTATUS(COMMA);
			if (__fly_zeros(*ptr))
				__FLY_AL_PARSE_PCSTATUS(ADD);
			goto perror;
		case WQ_ZERO_INT:
			if (__fly_point(*ptr))
				__FLY_AL_PARSE_PBSTATUS(WQ_ZERO_POINT);
			if (__fly_comma(*ptr))
				__FLY_AL_PARSE_PCSTATUS(COMMA);
			if (__fly_zeros(*ptr))
				__FLY_AL_PARSE_PCSTATUS(ADD);
			goto perror;
		case WQ_ZERO_POINT:
			if (fly_numeral(*ptr))
				__FLY_AL_PARSE_PCSTATUS(WQ_ZERO_DECIMAL_PLACE);

			goto perror;
		case WQ_ZERO_DECIMAL_PLACE:
			if (fly_numeral(*ptr) && decimal_places++ < 3)	break;
			if (__fly_comma(*ptr))
				__FLY_AL_PARSE_PCSTATUS(COMMA);
			if (__fly_zeros(*ptr))
				__FLY_AL_PARSE_PCSTATUS(ADD);

			goto perror;
		case COMMA:
			__FLY_AL_PARSE_PCSTATUS(ADD);
		case ADD:
			/* add new lang */
			{
				struct __fly_lang *__nl;
				__nl = fly_pballoc(l->request->pool, sizeof(struct __fly_lang));
				if (__nl == NULL)
					return __FLY_AL_PARSE_ERROR;
				__nl->quality_value = __fly_qvalue_from_str(qvalue);
				__fly_lname_cpy(__nl->lname, lang_str);
				__fly_check_of_asterisk(__nl);
				__fly_accept_lang_add(l, __nl);
			}
			__FLY_AL_PARSE_PCSTATUS(ADD_END);
		case ADD_END:
			if (__fly_space(*ptr))	break;
			if (__fly_comma(*ptr))	break;
			if (__fly_zeros(*ptr))
				__FLY_AL_PARSE_PCSTATUS(END);

			/* back to init */
			__FLY_AL_PARSE_PCSTATUS(INIT);
		case END:
			return __FLY_AL_PARSE_SUCCESS;
		default:
			return __FLY_AL_PARSE_ERROR;
		}
		ptr++;
	}
perror:
	return __FLY_AL_PARSE_PERROR;
}

__fly_static int __fly_accept_lang_parse(fly_request_t *req, fly_hdr_value *accept_lang)
{
	fly_lang_t *l;

	l = req->language;
	return __fly_al_parse(l, accept_lang);
}

int fly_accept_language(fly_request_t *req)
{
	fly_hdr_ci *header;
	fly_hdr_value *accept_lang;

	header = req->header;
	if (header == NULL)
		return -1;

	__fly_accept_lang_init(req);
	switch(__fly_accept_lang(header, &accept_lang)){
	case __FLY_ACCEPT_LANG_FOUND:
		return __fly_accept_lang_parse(req, accept_lang);
	case __FLY_ACCEPT_LANG_NOTFOUND:
		__fly_accept_lang_add_asterisk(req);
		return 0;
	case __FLY_ACCEPT_LANG_ERROR:
		return -1;
	default:
		FLY_NOT_COME_HERE
	}
	FLY_NOT_COME_HERE
	return -1;
}

static float __fly_qvalue_from_str(char *qvalue)
{
	if (!qvalue || __fly_one(*qvalue))
		return FLY_LANG_QVALUE_MAX;
	return atof(qvalue);
}

__fly_static void __fly_lname_cpy(char *dist, char *src)
{
	int i=0;

	while (__fly_lang(*src) && !__fly_comma(*src) && i++ < FLY_LANG_MAXLEN)
		*dist++ = *src++;

	*dist = '\0';
}

static inline void __fly_check_of_asterisk(struct __fly_lang *__nc)
{
	if (strcmp(__nc->lname, "*") == 0)
		__nc->asterisk = true;
	else
		__nc->asterisk = false;
}
