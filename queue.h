#ifndef _QUEUE_H
#define _QUEUE_H

struct fly_queue{
	struct fly_queue *next;
	struct fly_queue *prev;
};

#define fly_container_of(ptr, type , member)		\
	({	\
		const typeof( ((type *) 0)->member ) *__p = (ptr); \
		(type *) (char *) __p - offsetof(type, member);	\
	})

#endif
