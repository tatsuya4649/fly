#ifndef _QUEUE_H
#define _QUEUE_H

#include <stddef.h>
#include <stdbool.h>
#include "util.h"
#ifdef DEBUG
#include <stdio.h>
#endif

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

//static inline void fly_queue_swap(struct fly_queue *__q1, struct fly_queue *__q2)
//{
//#ifdef DEBUG
//	struct fly_queue *__dq1n, *__dq1p;
//	struct fly_queue *__dq2n, *__dq2p;
//
//	__dq1n = __q1->next;
//	__dq1p = __q1->prev;
//	__dq2n = __q2->next;
//	__dq2p = __q2->prev;
//#endif
//	struct fly_queue  *__tmp1n, *__tmp1p, *__tmp1pn, *__tmp1np;
//	struct fly_queue  *__tmp2n, *__tmp2p, *__tmp2pn, *__tmp2np;
//	__tmp1p = __q1->prev;
//	__tmp1n = __q1->next;
//	__tmp2p = __q2->prev;
//	__tmp2n = __q2->next;
//
//	__tmp1pn = __q1->prev->next;
//	__tmp1np = __q1->next->prev;
//	__tmp2pn = __q2->prev->next;
//	__tmp2np = __q2->next->prev;
//	__q1->prev->next = __tmp2pn;
//	__q1->next->prev = __tmp2np;
//	__q2->prev->next = __tmp1pn;
//	__q2->next->prev = __tmp1np;
//
//	__q1->prev = __tmp2p;
//	__q1->next = __tmp2n;
//	__q2->prev = __tmp1p;
//	__q2->next = __tmp1n;
//
//#ifdef DEBUG
//	printf("%p == %p\n", __q1->next, __dq2n);
//	printf("%p == %p\n", __q1->prev, __dq2p);
//	printf("%p == %p\n", __q2->next, __dq1n);
//	printf("%p == %p\n", __q2->prev, __dq1p);
//	assert(__q1->next == __dq2n);
//	assert(__q1->prev == __dq2p);
//	assert(__q2->next == __dq1n);
//	assert(__q2->prev == __dq1p);
//#endif
//	return;
//}

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
static inline void fly_queue_empty(struct fly_queue *__h)
{
	fly_queue_init(__h);
}

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

