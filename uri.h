#ifndef _URI_H
#define _URI_H

#define FLY_URI_INDEX_NAME				("index.html")
struct fly_uri{
	char *ptr;
	size_t len;
};

typedef struct fly_uri fly_uri_t;

#define fly_uri_set(__req, __ptr, __len)			\
	do {											\
		(__req)->request_line->uri.ptr = (__ptr);		\
		(__req)->request_line->uri.len = (__len);		\
	} while(0)

#endif
