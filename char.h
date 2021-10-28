#ifndef _CHAR_H
#define _CHAR_H

#include <stdbool.h>

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

static inline bool fly_question(char c)
{
	return c == 0x3F ? true : false;
}

#endif
