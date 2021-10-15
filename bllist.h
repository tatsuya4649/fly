#ifndef _BLLIST_H
#define _BLLIST_H

struct fly_bllist{
	struct fly_bllist *next;
	struct fly_bllist *prev;
};

static inline void fly_bllist_init(struct fly_bllist *__b)
{
	__b->next = __b;
	__b->prev = __b;
}

static inline void fly_bllist_add_tail(struct fly_bllist *__h, struct fly_bllist *__n)
{
	__n->prev = __h->prev;
	__n->next = __h;
	__h->prev->next = __n;
	__h->prev = __n;
}

static inline void fly_bllist_add_head(struct fly_bllist *__h, struct fly_bllist *__n)
{
	__n->prev = __h;
	__n->next = __h->next;
	__h->next->prev = __n;
	__h->next = __n;
}

static inline void fly_bllist_remove(struct fly_bllist *__n)
{
	__n->prev->next = __n->next;
	__n->next->prev = __n->prev;
}

#define fly_bllist_data(ptr, type, member)	\
			fly_container_of(ptr, type, member)

#define fly_for_each_bllist(__b, start)		\
		for (__b=(start)->next; __b!=start; __b=__b->next)

#endif
