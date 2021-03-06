#ifndef _CHAR_H
#define _CHAR_H

#include <stdbool.h>
#include <stddef.h>

static inline bool fly_equal(char c)
{
	return c == 0x3D ? true : false;
}

static inline bool fly_space(char c)
{
	return c == 0x20 ? true : false;
}

static inline bool fly_ht(char c)
{
	return c == 0x9 ? true : false;
}

static inline bool fly_sharp(char c)
{
	return c == 0x23 ? true : false;
}

static inline bool fly_atsign(char c)
{
	return c == 0x40 ? true : false;
}

static inline bool fly_minus(char c)
{
	return c == 0x2D ? true : false;
}

static inline bool fly_asterisk(char c)
{
	return c == 0x2A ? true : false;
}

static inline bool fly_cr(char c)
{
	return c == 0xD ? true : false;
}

static inline bool fly_lf(char c)
{
	return c == 0xA ? true : false;
}

static inline bool fly_underscore(char c)
{
	return c == 0x5F ? true : false;
}

static inline bool fly_ualpha(char c)
{
	return (c>=0x41 && c<=0x5A) ? true : false;
}

static inline bool fly_lalpha(char c)
{
	return (c>=0x61 && c<=0x7A) ? true : false;
}

static inline void fly_alpha_upper_to_lower(char *c)
{
	if (fly_ualpha(*c)){
		char __u = *c;
		*c = __u + 0x20;
	}else
		return;
}

static inline char fly_alpha_lower(char c)
{
	return fly_ualpha(c) ? c + 0x20 : c;
}


static inline bool fly_alpha(char c)
{
	return (fly_ualpha(c) || fly_lalpha(c)) ? true : false;
}

static inline bool fly_numeral(char c)
{
	return (c>=0x30 && c<=0x39) ? true : false;
}

static inline bool fly_alpha_numeral(char c)
{
	return (fly_alpha(c) || fly_numeral(c)) ? true : false;
}

static inline bool fly_slash(char c)
{
	return c == 0x2F ? true : false;
}

static inline bool fly_dot(char c)
{
	return c == 0x2E ? true : false;
}

static inline bool fly_colon(char c)
{
	return c == 0x3A ? true : false;
}

static inline bool fly_semicolon(char c)
{
	return c == 0x3B ? true : false;
}

static inline bool fly_question(char c)
{
	return c == 0x3F ? true : false;
}

static inline bool fly_tilda(char c)
{
	return c == 0x7E ? true : false;
}

static inline bool fly_hyphen(char c)
{
	return c == 0x2D ? true : false;
}

#define FLY_SLASH						'/'
#define FLY_QUESTION					'?'

/* Check whether string are same when ignore lower and upper. */
static inline bool fly_same_string_ignore_lu(char *c1, char *c2, size_t n)
{
	for (size_t i=0; i<n; i++){
		if (fly_alpha_lower(c1[i]) != fly_alpha_lower(c2[i]))
			return false;
	}
	return true;
}

#endif
