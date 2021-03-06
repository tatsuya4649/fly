#include "charset.h"
#include "request.h"
#include "header.h"
#include "char.h"

static void __fly_accept_charset_init(fly_request_t *req);
static void __fly_accept_add_asterisk(fly_request_t *req);
__fly_static void __fly_accept_add(fly_charset_t *cs, struct __fly_charset *__nc);
static inline bool __fly_vchar(char c);
static inline bool __fly_tchar(char c);
static inline bool __fly_token(char c);
static inline bool __fly_q(char c);
static inline bool __fly_zeros(char c);
static inline bool __fly_zero(char c);
static inline bool __fly_one(char c);
static inline bool __fly_comma(char c);

__fly_static int __fly_ac_parse(fly_charset_t *cs, fly_hdr_value *accept_charset);
__fly_static int __fly_accept_charset_parse(fly_request_t *req, fly_hdr_value *accept_charset);
static float __fly_qvalue_from_str(char *qvalue);
__fly_static void __fly_cname_cpy(char *dist, char *src);
static inline void __fly_check_of_asterisk(struct __fly_charset *__nc);
#define __FLY_ACCEPT_CHARSET_FOUND			(1)
#define __FLY_ACCEPT_CHARSET_NOTFOUND		(0)
#define __FLY_ACCEPT_CHARSET_ERROR			(-1)
__fly_static int __fly_accept_charset(fly_hdr_ci *header, fly_hdr_value **value);

__fly_static int __fly_accept_charset(fly_hdr_ci *header, fly_hdr_value **value)
{
	fly_hdr_c *__h;
	struct fly_bllist *__b;

	if (header->chain_count == 0)
		return __FLY_ACCEPT_CHARSET_NOTFOUND;

	fly_for_each_bllist(__b, &header->chain){
		__h = fly_bllist_data(__b, fly_hdr_c, blelem);
		if (__h->name_len>0 && \
				__h->value_len>0 && \
				strcmp(__h->name, FLY_ACCEPT_CHARSET) == 0){
			*value = __h->value;
			return __FLY_ACCEPT_CHARSET_FOUND;
		}
	}

	return __FLY_ACCEPT_CHARSET_NOTFOUND;
}

__fly_static void __fly_accept_add(fly_charset_t *cs, struct __fly_charset *__nc)
{
	fly_bllist_add_tail(&cs->charsets, &__nc->blelem);
	cs->charset_count++;
}

static void __fly_accept_add_asterisk(fly_request_t *req)
{
	struct __fly_charset *__cs;

	__cs = fly_pballoc(req->pool, sizeof(struct __fly_charset));
	strcpy(__cs->cname, "*");
	__cs->quality_value = FLY_CHARSET_QVALUE_MAX;
	__cs->asterisk = true;

	__fly_accept_add(req->charset, __cs);
}

static void __fly_accept_charset_init(fly_request_t *req)
{
	fly_charset_t *charset;
	fly_pool_t *pool;

	pool = req->pool;
	charset = fly_pballoc(pool, sizeof(fly_charset_t));
	charset->charset_count = 0;
	charset->request = req;
	req->charset = charset;
	fly_bllist_init(&charset->charsets);
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

static inline bool __fly_charset(char c)
{
	return __fly_token(c) ? true : false;
}

static inline bool __fly_q(char c)
{
	return (c == 'q') ? true : false;
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

static inline bool __fly_point(char c)
{
	return (c == 0x2E) ? true : false;
}

static inline bool __fly_comma(char c)
{
	return (c == 0x2C) ? true : false;
}

__fly_static int __fly_ac_parse(fly_charset_t *cs, fly_hdr_value *accept_charset)
{
#define __FLY_AC_PARSE_SUCCESS		1
#define __FLY_AC_PARSE_PERROR		0
#define __FLY_AC_PARSE_ERROR		-1
#define __FLY_AC_PARSE_PBSTATUS(s)		\
	{									\
		pstatus = (s);					\
		break;							\
	}
#define __FLY_AC_PARSE_PCSTATUS(s)		\
	{									\
		pstatus = (s);					\
		continue;							\
	}
	fly_hdr_value *ptr;
	enum{
		INIT,
		CHARSET,
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
	char *charset_str=NULL, *qvalue=NULL;
	int decimal_places=0;

	ptr = accept_charset;
	pstatus = INIT;
	while(true)
	{
		switch(pstatus){
		case INIT:
			charset_str = NULL;
			qvalue = NULL;
			decimal_places = 0;
			if (__fly_charset(*ptr)){
				charset_str = ptr;
				__FLY_AC_PARSE_PCSTATUS(CHARSET);
			}

			goto perror;
		case CHARSET:
			if (__fly_comma(*ptr))
				__FLY_AC_PARSE_PCSTATUS(COMMA);
			if (__fly_zeros(*ptr))
				__FLY_AC_PARSE_PCSTATUS(ADD);
			if (fly_semicolon(*ptr))
				__FLY_AC_PARSE_PBSTATUS(WSEMICOLON);
			if (fly_space(*ptr))
				__FLY_AC_PARSE_PCSTATUS(WOWS1);

			if (__fly_charset(*ptr)) break;
			goto perror;
		case WOWS1:
			if (fly_space(*ptr))	break;
			if (fly_semicolon(*ptr))
					__FLY_AC_PARSE_PBSTATUS(WSEMICOLON);
			goto perror;
		case WSEMICOLON:
			if (fly_space(*ptr))
				__FLY_AC_PARSE_PBSTATUS(WOWS2);
			if (__fly_q(*ptr))
				__FLY_AC_PARSE_PBSTATUS(WQ);

			goto perror;
		case WOWS2:
			if (fly_space(*ptr))	break;
			if (__fly_q(*ptr))
				__FLY_AC_PARSE_PBSTATUS(WQ);

			goto perror;
		case WQ:
			if (fly_equal(*ptr))
				__FLY_AC_PARSE_PBSTATUS(WQEQUAL);

			goto perror;
		case WQEQUAL:
			if (__fly_one(*ptr)){
				/* start of quality value */
				qvalue = ptr;
				__FLY_AC_PARSE_PBSTATUS(WQ_ONE_INT);
			}
			if (__fly_zero(*ptr)){
				/* start of quality value */
				qvalue = ptr;
				__FLY_AC_PARSE_PBSTATUS(WQ_ZERO_INT);
			}

			goto perror;
		case WQ_ONE_INT:
			if (__fly_point(*ptr))
				__FLY_AC_PARSE_PBSTATUS(WQ_ONE_POINT);
			if (__fly_comma(*ptr))
				__FLY_AC_PARSE_PCSTATUS(COMMA);
			if (__fly_zeros(*ptr))
				__FLY_AC_PARSE_PCSTATUS(ADD);
			goto perror;
		case WQ_ONE_POINT:
			if (__fly_zero(*ptr))
				__FLY_AC_PARSE_PCSTATUS(WQ_ONE_DECIMAL_PLACE);
			goto perror;
		case WQ_ONE_DECIMAL_PLACE:
			if (__fly_zero(*ptr) && decimal_places++ < 3)	break;
			if (__fly_comma(*ptr))
				__FLY_AC_PARSE_PCSTATUS(COMMA);
			if (__fly_zeros(*ptr))
				__FLY_AC_PARSE_PCSTATUS(ADD);
			goto perror;
		case WQ_ZERO_INT:
			if (__fly_point(*ptr))
				__FLY_AC_PARSE_PBSTATUS(WQ_ZERO_POINT);
			if (__fly_comma(*ptr))
				__FLY_AC_PARSE_PCSTATUS(COMMA);
			if (__fly_zeros(*ptr))
				__FLY_AC_PARSE_PCSTATUS(ADD);
			goto perror;
		case WQ_ZERO_POINT:
			if (fly_numeral(*ptr))
				__FLY_AC_PARSE_PCSTATUS(WQ_ZERO_DECIMAL_PLACE);

			goto perror;
		case WQ_ZERO_DECIMAL_PLACE:
			if (fly_numeral(*ptr) && decimal_places++ < 3)	break;
			if (__fly_comma(*ptr))
				__FLY_AC_PARSE_PCSTATUS(COMMA);
			if (__fly_zeros(*ptr))
				__FLY_AC_PARSE_PCSTATUS(ADD);

			goto perror;
		case COMMA:
			__FLY_AC_PARSE_PCSTATUS(ADD);
		case ADD:
			/* add new charset */
			{
				struct __fly_charset *__nc;
				__nc = fly_pballoc(cs->request->pool, sizeof(struct __fly_charset));
				if (__nc == NULL)
					return __FLY_AC_PARSE_ERROR;
				__nc->quality_value = __fly_qvalue_from_str(qvalue);
				__fly_cname_cpy(__nc->cname, charset_str);
				__fly_check_of_asterisk(__nc);
				__fly_accept_add(cs, __nc);
			}
			__FLY_AC_PARSE_PCSTATUS(ADD_END);
		case ADD_END:
			if (fly_space(*ptr))	break;
			if (__fly_comma(*ptr))	break;
			if (__fly_zeros(*ptr))
				__FLY_AC_PARSE_PCSTATUS(END);

			/* back to init */
			__FLY_AC_PARSE_PCSTATUS(INIT);
		case END:
			return __FLY_AC_PARSE_SUCCESS;
		default:
			return __FLY_AC_PARSE_ERROR;
		}
		ptr++;
	}
perror:
	return __FLY_AC_PARSE_PERROR;
}

__fly_static int __fly_accept_charset_parse(fly_request_t *req, fly_hdr_value *accept_charset)
{
	fly_charset_t *cs;

	cs = req->charset;
	return __fly_ac_parse(cs, accept_charset);
}

int fly_accept_charset(fly_request_t *req)
{
	fly_hdr_ci *header;
	fly_hdr_value *accept_charset;
	struct fly_bllist *__b;
	struct __fly_charset *__c;

	header = req->header;
	if (header == NULL)
		return FLY_ACCEPT_CHARSET_ERROR;

	accept_charset = NULL;
	__fly_accept_charset_init(req);
	switch(__fly_accept_charset(header, &accept_charset)){
	case __FLY_ACCEPT_CHARSET_FOUND:
		switch(__fly_accept_charset_parse(req, accept_charset)){
		case __FLY_AC_PARSE_SUCCESS:
			break;
		case __FLY_AC_PARSE_PERROR:
			return FLY_ACCEPT_CHARSET_SYNTAX_ERROR;
		case __FLY_AC_PARSE_ERROR:
			return FLY_ACCEPT_CHARSET_ERROR;
		default:
			FLY_NOT_COME_HERE
		}
		break;
	case __FLY_ACCEPT_CHARSET_NOTFOUND:
		__fly_accept_add_asterisk(req);
		return FLY_ACCEPT_CHARSET_SUCCESS;
	case __FLY_ACCEPT_CHARSET_ERROR:
		return FLY_ACCEPT_CHARSET_ERROR;
	default:
		FLY_NOT_COME_HERE
	}

	if (req->charset == NULL || req->charset->charset_count == 0)
		return FLY_ACCEPT_CHARSET_SYNTAX_ERROR;

	fly_for_each_bllist(__b, &req->charset->charsets){
		__c = fly_bllist_data(__b, struct __fly_charset,blelem);
		if (__c->quality_value > (float) 0)
			return FLY_ACCEPT_CHARSET_SUCCESS;
	}
	return FLY_ACCEPT_CHARSET_NOT_ACCEPTABLE;
}

static float __fly_qvalue_from_str(char *qvalue)
{
	if (!qvalue || __fly_one(*qvalue))
		return FLY_CHARSET_QVALUE_MAX;
	return atof(qvalue);
}

__fly_static void __fly_cname_cpy(char *dist, char *src)
{
	int i=0;

	while (__fly_charset(*src) && !__fly_comma(*src) && i++ < FLY_CHARSET_MAXLEN)
		*dist++ = *src++;

	*dist = '\0';
}

static inline void __fly_check_of_asterisk(struct __fly_charset *__nc)
{
	if (strcmp(__nc->cname, "*") == 0)
		__nc->asterisk = true;
	else
		__nc->asterisk = false;
}
