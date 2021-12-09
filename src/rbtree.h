#ifndef _RBTREE_H
#define _RBTREE_H

#include <stdbool.h>
#include "alloc.h"
#include "util.h"

union __fly_rbdata;
struct fly_rb_node;
typedef int (*fly_rb_cmp_t)(union __fly_rbdata *k1, union __fly_rbdata *k2, union __fly_rbdata *data);
struct fly_rb_tree{
	struct fly_rb_root *root;
	size_t node_count;
	/* decide to which data is bigger */
#define FLY_RB_CMP_BIG			(1)
#define FLY_RB_CMP_EQUAL		(0)
#define FLY_RB_CMP_SMALL		(-1)
	fly_rb_cmp_t cmp;
};
typedef struct fly_rb_tree fly_rb_tree_t;
typedef struct fly_rb_node fly_rb_node_t;

struct fly_rb_root{
	struct fly_rb_node *node;
};

union __fly_rbdata{
	void 		*__p;
	size_t		__s;
	int			__i;
	long		__l;
};
typedef union __fly_rbdata fly_rbdata_t;
#define fly_rbdata_set_ptr(__rd, __v)		((__rd)->__p = __v)
#define fly_rbdata_set_size(__rd, __v)		((__rd)->__s = __v)
#define fly_rbdata_set_int(__rd, __v)		((__rd)->__i = __v)
#define fly_rbdata_set_long(__rd, __v)		((__rd)->__l = __v)
#define fly_rbdata_ptr(__rd)				((__rd)->__p)
#define fly_rbdata_size(__rd)				((__rd)->__s)
#define fly_rbdata_int(__rd)				((__rd)->__i)
#define fly_rbdata_long(__rd)				((__rd)->__l)

struct fly_rb_node{
	/* use for compare node */
	//void 				*key;
	/* node data */
	//void *data;
#define __fly_rbdata_get(__n, name)				\
			((__n)->key.name)
#define __fly_rbdata_set(__n, name, __v)		\
			((__n)->key.name = __v)
#define fly_rbkey_get(__n, name)				\
			__fly_rbdata_get(__n, name)
#define fly_rbkey_set(__n, name, __v)				\
			__fly_rbdata_set(__n, name, __v)
#define fly_rbdata_get(__n, name)				\
			__fly_rbdata_get(__n, name)
#define fly_rbdata_set(__n, name, __v)				\
			__fly_rbdata_set(__n, name, __v)
	fly_rbdata_t				key;
	fly_rbdata_t				data;
	struct fly_rb_node 			**node_data;
	struct fly_rb_node 			*parent;
	struct fly_rb_node 			*c_right;
	struct fly_rb_node 			*c_left;
	fly_bit_t 					color: 1;
};

extern struct fly_rb_node nil_node;
#define nil_node_ptr		&nil_node
typedef int fly_rb_color_t;
#define FLY_RB_BLACK		(0)
#define FLY_RB_RED			(1)
#define FLY_RB_UNKNOWN		(FLY_RB_BLACK)

struct fly_rb_tree *fly_rb_tree_init(fly_rb_cmp_t cmp);
void fly_rb_tree_release(struct fly_rb_tree *tree);
fly_rb_node_t *fly_rb_tree_insert(struct fly_rb_tree *tree, fly_rbdata_t *data, fly_rbdata_t *key, struct fly_rb_node **node_data, fly_rbdata_t *__cmpdata);
struct fly_rb_node *fly_rb_tree_insert_node(struct fly_rb_tree *tree, struct fly_rb_node *node, fly_rbdata_t *cmpdata);
void fly_rb_delete(struct fly_rb_tree *tree, struct fly_rb_node *node);
fly_rb_node_t *fly_rb_node_from_key(struct fly_rb_tree *tree, fly_rbdata_t *key, fly_rbdata_t *cmpdata);
fly_rbdata_t *fly_rb_node_data_from_key(struct fly_rb_tree *tree, fly_rbdata_t *key, fly_rbdata_t *data);

#endif
