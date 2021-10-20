#ifndef _RBTREE_H
#define _RBTREE_H

#include <stdbool.h>
#include "alloc.h"
#include "util.h"

struct fly_rb_node;
typedef int (*fly_rb_cmp_t)(void *d1, void *d2);
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

struct fly_rb_node{
	/* use for compare node */
	void *key;
	/* node data */
	void *data;
	struct fly_rb_node **node_data;
	struct fly_rb_node *parent;
	struct fly_rb_node *c_right;
	struct fly_rb_node *c_left;
	fly_bit_t color: 1;
};

extern struct fly_rb_node nil_node;
#define nil_node_ptr		&nil_node
typedef int fly_rb_color_t;
#define FLY_RB_BLACK		(0)
#define FLY_RB_RED			(1)
#define FLY_RB_UNKNOWN		(FLY_RB_BLACK)

struct fly_rb_tree *fly_rb_tree_init(fly_rb_cmp_t cmp);
void fly_rb_tree_release(struct fly_rb_tree *tree);
fly_rb_node_t *fly_rb_tree_insert(struct fly_rb_tree *tree, void *data, void *key, struct fly_rb_node **node_data);
void fly_rb_tree_insert_node(struct fly_rb_tree *tree, struct fly_rb_node *node);
void fly_rb_delete(struct fly_rb_tree *tree, struct fly_rb_node *node);
fly_rb_node_t *fly_rb_node_from_key(struct fly_rb_tree *tree, void *key);
void *fly_rb_node_data_from_key(struct fly_rb_tree *tree, void *key);

#endif
