#ifndef _QUEUE_H
#define _QUEUE_H

#include <stddef.h>
#include <stdbool.h>
#include "util.h"

struct fly_queue{
	int				 count;
	struct fly_queue *head;
	struct fly_queue *next;
	struct fly_queue *prev;
};

static inline int fly_queue_count(struct  fly_queue *__h)
{
	return __h->count;
}

static inline void fly_queue_push(struct fly_queue *__h, struct fly_queue *n) {
	n->prev = __h->prev;
	n->next = __h;
	__h->prev->next = n;
	__h->prev = n;
	__h->count++;
	n->head = __h;
}

static inline struct fly_queue *fly_queue_pop(struct fly_queue *__h)
{
	struct fly_queue *l = __h->prev;

	l->prev->next = l->next;
	l->next->prev = l->prev;
	__h->count--;
	return l;
}

static inline struct fly_queue *fly_queue_last(struct fly_queue *__h)
{
	return __h->prev;
}

static inline void fly_queue_remove(struct fly_queue *__x)
{
	__x->next->prev = __x->prev;
	__x->prev->next = __x->next;
	__x->head->count--;
}

static inline void fly_queue_init(struct fly_queue *__h)
{
	__h->count = 0;
	__h->next = __h;
	__h->prev = __h;
	__h->head = __h;
}
__attribute__((weak, alias("fly_queue_init"))) void fly_queue_empty(struct fly_queue *__h);

#define fly_queue_data(ptr, type, member)			\
	fly_container_of(ptr, type, member)

static inline bool fly_is_queue_empty(struct fly_queue *__h)
{
	return __h->next == __h ? true : false;
}

#define fly_for_each_queue(__q, start)		\
		for (__q=(start)->next; __q!=start; __q=__q->next)
#define fly_for_each_queue_data()			\

#endif

