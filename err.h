#ifndef _FLY_ERR_H
#define _FLY_ERR_H

#include <errno.h>

/* errno */
#ifndef FLY_ERROR
#define FLY_ERROR(errno)		(-1*errno)
#endif
/* Success */
#define FLY_SUCCESS				0
/* params error */
#define FLY_EARG				-1
/* not found error */
#define FLY_ENOTFOUND			-2

#endif
