#include "rbtree.h"

#define nil_node_ptr		&nil_node
struct fly_rb_node nil_node = {
	.c_right = nil_node_ptr,
	.c_left = nil_node_ptr,
	.parent = nil_node_ptr,
	.color = FLY_RB_BLACK,
	.data = NULL,
};

static inline void fly_rb_color_update(struct fly_rb_node *node, fly_rb_color_t color);
static inline void fly_rb_subst(struct fly_rb_node **dist, struct fly_rb_node **src);
static inline bool fly_rb_no_child(struct fly_rb_node *node);
static inline bool fly_rb_all_child(struct fly_rb_node *node);
static inline bool fly_rb_part_child(struct fly_rb_node *node);
static inline void fly_rb_root(struct fly_rb_tree *tree, struct fly_rb_node *node);
#ifdef DEBUG
enum __fly_rbtree_debug_type{
	__FLY_RBTREE_DEBUG_DELETE,
	__FLY_RBTREE_DEBUG_INSERT,
	__FLY_RBTREE_DEBUG_NOCHILD,
	__FLY_RBTREE_DEBUG_NORIGHT,
	__FLY_RBTREE_DEBUG_TWOCHILD
};

void __fly_rbtree_debug(fly_rb_tree_t *tree, enum __fly_rbtree_debug_type type __unused);
void __fly_rbtree_node_debug(fly_rb_tree_t *tree __unused, fly_rb_node_t *node, int *black_count);
#define FLY_RBTREE_NODE_FOUND			1
#define FLY_RBTREE_NODE_NOTFOUND		0
int __fly_rbtree_node_in_tree(fly_rb_node_t *node, fly_rb_node_t *target);
void __fly_rbtree_free_node_in_node_data(fly_rb_node_t *node, fly_rb_node_t *freed, enum __fly_rbtree_debug_type type __unused);
void __fly_rbtree_node_in_node_data(fly_rb_node_t *node);
#endif

struct fly_rb_tree *fly_rb_tree_init(fly_rb_cmp_t cmp)
{
    struct fly_rb_tree *t;

    t = fly_malloc(sizeof(struct fly_rb_tree));
    t->root = NULL;
    t->node_count = 0;
	t->cmp = cmp;
    if (fly_unlikely_null(t))
        return NULL;

    return t;
}

__fly_static void __fly_rb_tree_release(fly_rb_tree_t *tree, fly_rb_node_t *node)
{
	if (node->c_left != nil_node_ptr)
		__fly_rb_tree_release(tree, node->c_left);
	if (node->c_right != nil_node_ptr)
		__fly_rb_tree_release(tree, node->c_right);

	tree->node_count--;
	fly_free(node);
}

void fly_rb_tree_release(struct fly_rb_tree *tree)
{
	fly_rb_node_t *__n;
	if (tree->node_count == 0){
		fly_free(tree);
		return;
	}else{
		__n = tree->root->node;
		__fly_rb_tree_release(tree, __n);
		fly_free(tree->root);
	}
	fly_free(tree);
}

static inline int fly_rb_tree_node_count(struct fly_rb_tree *tree)
{
	return tree->node_count;
}

static struct fly_rb_root *fly_rb_root_init(struct fly_rb_tree *tree, struct fly_rb_node *node)
{
	struct fly_rb_root *r = fly_malloc(sizeof(struct fly_rb_root));
	if (fly_unlikely_null(r))
		return NULL;

	tree->root = r;
	fly_rb_root(tree, node);
	tree->node_count = 1;
	return r;
}

static struct fly_rb_node *fly_rb_min_from_node(struct fly_rb_node *node)
{
	if (node->c_right == nil_node_ptr)
		return nil_node_ptr;
	else{
		struct fly_rb_node *__m;

		__m = node->c_right;
        while(__m->c_left != nil_node_ptr)
            __m = __m->c_left;

		return __m;
	}
}

static void fly_rb_root_release(struct fly_rb_tree *tree)
{
	if (tree->root){
		tree->node_count = 0;
		fly_free(tree->root->node);
		fly_free(tree->root);
	}
}

static inline bool fly_rb_node_is_root(struct fly_rb_tree *tree, struct fly_rb_node *node)
{
    return tree->root->node == node ? true : false;
}

static inline void fly_rb_root(struct fly_rb_tree *tree, struct fly_rb_node *node)
{
	tree->root->node = node;
	node->parent = nil_node_ptr;
	fly_rb_color_update(node, FLY_RB_BLACK);
}

static inline void fly_rb_parent(struct fly_rb_node *node, struct fly_rb_node *parent)
{
	if (node != nil_node_ptr)
		node->parent = parent;
}

static inline bool fly_rb_parent_is_red(struct fly_rb_node *node)
{
    return node->parent->color & FLY_RB_RED ? true : false;
}

static inline bool fly_rb_parent_is_black(struct fly_rb_node *node)
{
    return node->parent->color == FLY_RB_BLACK? true : false;
}


static inline void fly_rb_reverse_color(struct fly_rb_node *node)
{
    node->color = ~node->color;
}

struct fly_rb_node *fly_rb_get_uncle(struct fly_rb_node *node)
{
    if (node->parent == nil_node_ptr)
        return nil_node_ptr;
    else if (node->parent->parent == nil_node_ptr)
        return nil_node_ptr;

    struct fly_rb_node *__g = node->parent->parent;
    if (__g->c_left == node->parent)
        return __g->c_right;
    else if (__g->c_right == node->parent)
        return __g->c_left;
	else
		FLY_NOT_COME_HERE;
}


static inline bool fly_is_red(fly_rb_color_t __c)
{
	return __c & FLY_RB_RED ? true : false;
}

static inline bool fly_is_black(fly_rb_color_t __c)
{
	return !fly_is_red(__c);
}

__unused static bool fly_rb_uncle_is_red(struct fly_rb_node *node)
{
    return fly_is_red(fly_rb_get_uncle(node)->color);
}

__unused static bool fly_rb_uncle_is_black(struct fly_rb_node *node)
{
    return fly_is_black(fly_rb_get_uncle(node)->color);
}

static bool fly_rb_node_is_left(struct fly_rb_node *node)
{
    return node->parent->c_left == node ? true : false;
}

static bool fly_rb_node_is_right(struct fly_rb_node *node)
{
    return !fly_rb_node_is_left(node);
}

static inline void fly_rb_node_parent(struct fly_rb_node *node, struct fly_rb_node *parent)
{
    node->parent = parent;
}

static void fly_rb_rotate_left(struct fly_rb_node *node, struct fly_rb_tree *tree)
{
    struct fly_rb_node *right;

    right = node->c_right;

	assert(right != nil_node_ptr);
	node->c_right = right->c_left;
	if (node->c_right != nil_node_ptr)
		fly_rb_node_parent(node->c_right, node);

	fly_rb_node_parent(right, node->parent);

    if (fly_rb_node_is_root(tree, node))
		fly_rb_root(tree, right);
	else if (fly_rb_node_is_left(node))
		node->parent->c_left = right;
	else
		node->parent->c_right = right;

	right->c_left = node;
    fly_rb_node_parent(node, right);
}

static void fly_rb_rotate_right(struct fly_rb_node *node, struct fly_rb_tree *tree)
{
    struct fly_rb_node *left;

    left = node->c_left;

	assert(left != nil_node_ptr);
	node->c_left =  left->c_right;
	if (node->c_left != nil_node_ptr)
        fly_rb_node_parent(node->c_left, node);

	fly_rb_node_parent(left, node->parent);

    if (fly_rb_node_is_root(tree, node))
		fly_rb_root(tree, left);
	else if (fly_rb_node_is_left(node))
		node->parent->c_left = left;
	else
		node->parent->c_right = left;

	left->c_right = node;
    fly_rb_node_parent(node, left);
}

static inline void fly_rb_subst(struct fly_rb_node **dist, struct fly_rb_node **src)
{
	*dist = *src;
}

static inline void fly_rb_color_update(struct fly_rb_node *node, fly_rb_color_t color)
{
    /* nil node color must not change */
    if (node == nil_node_ptr)   return;
    node->color = color;
}

fly_rb_node_t *__fly_node_init(void *data, void *key)
{
	fly_rb_node_t *node;

	node = fly_malloc(sizeof(fly_rb_node_t));
	if (fly_unlikely_null(node))
		return NULL;

	node->data = data;
	node->key = key;
	node->parent = nil_node_ptr;
	node->c_right = nil_node_ptr;
	node->c_left = nil_node_ptr;
	node->node_data = NULL;
	fly_rb_color_update(node, FLY_RB_RED);

	return node;
}

__fly_static int __fly_rb_node_from_key(struct fly_rb_tree *tree, struct fly_rb_node *node, void *key, void *data)
{
	return tree->cmp(key, node->key, data);
}

void *fly_rb_node_data_from_key(struct fly_rb_tree *tree, void *key, void *data)
{
	fly_rb_node_t *__n;

	__n = fly_rb_node_from_key(tree, key, data);
	if (fly_unlikely_null(__n))
		return NULL;
	else
		return __n->data;
}

fly_rb_node_t *fly_rb_node_from_key(struct fly_rb_tree *tree, void *key, void *data)
{
	struct fly_rb_node *__n;
	if (!tree->cmp)
		return NULL;

	__n = tree->root->node;
	while(__n != nil_node_ptr){
		switch(__fly_rb_node_from_key(tree, __n, key, data)){
		case FLY_RB_CMP_BIG:
			__n = __n->c_right;
			break;
		case FLY_RB_CMP_EQUAL:
			return __n;
		case FLY_RB_CMP_SMALL:
			__n = __n->c_left;
			break;
		}
	}
	return NULL;
}

fly_rb_node_t *fly_rb_tree_insert(struct fly_rb_tree *tree, void *data, void *key, struct fly_rb_node **node_data, void *__cmpdata)
{
	fly_rb_node_t *node;

	node = __fly_node_init(data, key);
	if (fly_unlikely_null(node))
		return NULL;

#ifdef DEBUG
	size_t count = tree->node_count;
	assert(tree!=NULL);
	struct fly_rb_node *tmp;

	tmp = node;
	if (tree->node_count > 0)
		__fly_rbtree_node_in_node_data(tree->root->node);
#endif
	node->node_data = node_data;
	if (node->node_data != NULL)
		*node->node_data = node;
	node = fly_rb_tree_insert_node(tree, node, __cmpdata);

#ifdef DEBUG
	int ret;

	__fly_rbtree_debug(tree, __FLY_RBTREE_DEBUG_INSERT);
	assert(tree->node_count > 0);

	if (tmp == node)
		assert(tree->node_count == (count+1));

	if (!fly_rb_node_is_root(tree, node)){
		ret = __fly_rbtree_node_in_tree(tree->root->node, node);
		assert(ret == FLY_RBTREE_NODE_FOUND);
	}
	__fly_rbtree_node_in_node_data(tree->root->node);
#endif
	return node;
}

struct fly_rb_node *fly_rb_tree_insert_node(struct fly_rb_tree *tree, struct fly_rb_node *node, void *data)
{
	struct fly_rb_node *origin_node = node;
    struct fly_rb_node *__n, *__p, *__g, *__u;

    if (tree->node_count == 0){
        tree->root = fly_rb_root_init(tree, node);
        return origin_node;
    }

    __n = tree->root->node;
    while(true){
		switch(tree->cmp(node->key, __n->key, data)){
		case FLY_RB_CMP_EQUAL:
			fly_free(node);
            return __n;
		case FLY_RB_CMP_SMALL:
        /* go left */
            if (__n->c_left == nil_node_ptr){
				fly_rb_node_parent(node, __n);
                __n->c_left = node;
                break;
            }else
                __n = __n->c_left;
			continue;
        /* go right */
		case FLY_RB_CMP_BIG:
            if (__n->c_right == nil_node_ptr){
				fly_rb_node_parent(node, __n);
                __n->c_right = node;
                break;
            }else
                __n = __n->c_right;
			continue;
		default:
			FLY_NOT_COME_HERE
        }
		break;
    }

    while((__p=node->parent)!=nil_node_ptr && fly_is_red(__p->color)){
		/* grandparent node must be black */
        __g = __p->parent;
		/* parent left */
        if (fly_rb_node_is_left(__p)){
			__u = fly_rb_get_uncle(node);

			if (fly_is_red(__u->color)){
				fly_rb_color_update(__u, FLY_RB_BLACK);
				fly_rb_color_update(__p, FLY_RB_BLACK);
				fly_rb_color_update(__g, FLY_RB_RED);
				node = __g;
			}else{
				if (fly_rb_node_is_right(node)){
					node = __p;
					fly_rb_rotate_left(node, tree);
				}

			   fly_rb_color_update(node->parent, FLY_RB_BLACK);
			   fly_rb_color_update(node->parent->parent, FLY_RB_RED);
			   fly_rb_rotate_right(node->parent->parent, tree);
			}
		/* parent right */
        }else{
			__u = fly_rb_get_uncle(node);

			if (fly_is_red(__u->color)){
				fly_rb_color_update(__u, FLY_RB_BLACK);
				fly_rb_color_update(__p, FLY_RB_BLACK);
				fly_rb_color_update(__g, FLY_RB_RED);
				node = __g;
            }else{
				if (fly_rb_node_is_left(node)){
					node = __p;
					fly_rb_rotate_right(node, tree);
				}

				fly_rb_color_update(node->parent, FLY_RB_BLACK);
				fly_rb_color_update(node->parent->parent, FLY_RB_RED);
				fly_rb_rotate_left(node->parent->parent, tree);
			}
        }
    }

    tree->node_count++;
	if (fly_rb_node_is_root(tree, node))
		fly_rb_color_update(node, FLY_RB_BLACK);
    return origin_node;
}

static inline void fly_rb_swap(fly_rb_tree_t *tree, struct fly_rb_node *dist, struct fly_rb_node *src)
{
	struct fly_rb_node __tmp;

	__tmp.key = src->key;
	__tmp.data = src->data;
	__tmp.c_left = src->c_left;
	__tmp.c_right = src->c_right;
	__tmp.color = src->color;
	__tmp.parent = src->parent;

	if (src->c_left != dist)
		fly_rb_parent(src->c_left, dist);
	if (src->c_right != dist)
		fly_rb_parent(src->c_right, dist);
	src->c_left = dist->c_left!=src ? dist->c_left : dist;
	src->c_right = dist->c_right!=src ? dist->c_right : dist;
	src->key = dist->key;
	src->data = dist->data;
	fly_rb_color_update(src, dist->color);

	if (dist->c_left != src)
		fly_rb_parent(dist->c_left, src);
	if (dist->c_right != src)
		fly_rb_parent(dist->c_right, src);
	dist->c_left = __tmp.c_left!=dist ? __tmp.c_left : src;
	dist->c_right = __tmp.c_right!=dist ? __tmp.c_right : src;
	dist->key = __tmp.key;
	dist->data = __tmp.data;
	fly_rb_color_update(dist, __tmp.color);
	fly_rb_parent(src, dist->parent!=src ? dist->parent : dist);
	fly_rb_parent(dist, __tmp.parent!=dist ? __tmp.parent : src);

	if (fly_rb_node_is_root(tree, src))
		fly_rb_root(tree, dist);
	else if (fly_rb_node_is_root(tree, dist))
		fly_rb_root(tree, src);

	if (!fly_rb_node_is_root(tree, src)){
		if (fly_rb_node_is_left(src))
			src->parent->c_left = src;
		else
			src->parent->c_right = src;
	}
	if (!fly_rb_node_is_root(tree, dist)){
		if (fly_rb_node_is_left(dist))
			dist->parent->c_left = dist;
		else
			dist->parent->c_right = dist;
	}
}

static inline struct fly_rb_node *fly_rb_sibling(struct fly_rb_node *node)
{
    if (node->parent->c_left == node)
        return node->parent->c_right;
    else
        return node->parent->c_left;
}

static inline bool fly_rb_have_right_child(struct fly_rb_node *node)
{
	return node->c_right != nil_node_ptr ? true : false;
}

__fly_static void __fly_rb_delete_rebalance(struct fly_rb_tree *tree, struct fly_rb_node *node, struct fly_rb_node *parent)
{
	if (fly_is_red(node->color)){
		fly_rb_color_update(node, FLY_RB_BLACK);
		return;
	}else{
	/* node/old are black */
		struct fly_rb_node *__p, *__s;

		__p = parent;
		__s = (__p->c_left==node) ? __p->c_right : __p->c_left;
recursion:
		if (fly_rb_node_is_root(tree, node))
			return;

		/* node and child is black. */
		/* sibling is red */
		if (fly_is_red(__s->color)){
			/*
			 *	   P(B)						S(B)
			 *	   / \						/ \
			 *	N(B)  S(R)		->		 P(R)  Sr(B)
			 *		  / \				 / \
			 *	 Sl(B)   Sr(B)		  N(B)	Sl(B)
			 */
			if (__p->c_left == node){
				fly_rb_rotate_left(__p, tree);
			/*
			 *		   P(B)					S(B)
			 *		   / \					/  \
			 *		S(R)  N(R)	->		Sl(B)	P(R)
			 *		/ \							/ \
			 *	Sl(B)   Sr(B)				Sr(B)  N(B)
			 */
			}else if (__p->c_right == node){
				fly_rb_rotate_right(__p, tree);
			}else
				FLY_NOT_COME_HERE

			/* parent become red */
			fly_rb_color_update(__p, FLY_RB_RED);
			/* sibling become black */
			fly_rb_color_update(__s, FLY_RB_BLACK);

			__s = (__p->c_left==node) ? __p->c_right : __p->c_left;
			goto recursion;
		/* sibling is black */
		}else{
			/*
			 *	   P(B)
			 *	   / \
			 *	N(B)  S(B)
			 *		  /  \
			 *	  Sl(B)   Sr(B)
			 */
			if (fly_is_black(__p->color) && fly_is_black(__s->c_left->color) && fly_is_black(__s->c_right->color)){
				fly_rb_color_update(__s, FLY_RB_RED);

				node = __p;
				__p = node->parent;
				__s = (__p->c_left==node) ? __p->c_right : __p->c_left;
				goto recursion;
			/*
			 *	   P(R)
			 *	   / \
			 *	N(B)  S(B)
			 *		  /  \
			 *	  Sl(B)   Sr(B)
			 */
			} else if (fly_is_red(__p->color) && fly_is_black(__s->c_left->color) && fly_is_black(__s->c_right->color)){
				fly_rb_color_update(__p, FLY_RB_BLACK);
				fly_rb_color_update(__s, FLY_RB_RED);
				return;
			/*
			 *	   P(B or B)
			 *	   / \
			 *	N(B)  S(B)
			 *		  /  \
			 *	  Sl(R)   Sr(B)
			 */
			} else if (fly_is_red(__s->c_left->color) && fly_is_black(__s->c_right->color) && __p->c_left == node){
				fly_rb_node_t *__sl = __s->c_left;
				fly_rb_rotate_right(__s, tree);
				fly_rb_color_update(__s, FLY_RB_RED);
				fly_rb_color_update(__sl, FLY_RB_BLACK);

				__s = __sl;
				goto recursion;
			/*
			 *		   P(B or B)
			 *		   / \
			 *		S(B)  N(B)
			 *		/  \
			 *	Sl(B)   Sr(R)
			 */
			} else if (fly_is_black(__s->c_left->color) && fly_is_red(__s->c_right->color) && __p->c_right == node){
				fly_rb_node_t *__sr = __s->c_right;
				fly_rb_rotate_left(__s, tree);
				fly_rb_color_update(__s, FLY_RB_RED);
				fly_rb_color_update(__sr, FLY_RB_BLACK);

				__s = __sr;
				goto recursion;
			/*
			 *		   P(B or B)
			 *		   / \
			 *		N(B)  S(B)
			 *			 /	  \
			 *	 Sl(B or R)  Sr(R)
			 */
			} else if (fly_is_red(__s->c_right->color) && __p->c_left == node){
				fly_rb_color_t __pcolor = __p->color;
				fly_rb_color_t __scolor = __s->color;

				fly_rb_rotate_left(__p, tree);
				fly_rb_color_update(__p, __scolor);
				fly_rb_color_update(__s, __pcolor);
				fly_rb_color_update(__s->c_right, FLY_RB_BLACK);

				return;
			/*
			 *			   P(B or B)
			 * 			   / \
			 * 			S(B)  N(B)
			 * 			/  \
			 * 		Sl(R)   Sr(B or R)
			 */
			} else if (fly_is_red(__s->c_left->color) && __p->c_right == node){
				fly_rb_color_t __pcolor = __p->color;
				fly_rb_color_t __scolor = __s->color;

				fly_rb_rotate_right(__p, tree);
				fly_rb_color_update(__p, __scolor);
				fly_rb_color_update(__s, __pcolor);
				fly_rb_color_update(__s->c_left, FLY_RB_BLACK);

				return;
			}
		}
	}
}

static inline bool fly_rb_no_child(struct fly_rb_node *node)
{
    return node->c_left == nil_node_ptr && node->c_right == nil_node_ptr;
}

static inline bool fly_rb_all_child(struct fly_rb_node *node)
{
    return node->c_left != nil_node_ptr && node->c_right != nil_node_ptr;
}

static inline bool fly_rb_part_child(struct fly_rb_node *node)
{
	return !fly_rb_no_child(node) && !fly_rb_all_child(node);
}

#ifdef DEBUG
#include <stdio.h>
#endif
void fly_rb_delete(struct fly_rb_tree *tree, struct fly_rb_node *node)
{
#ifdef DEBUG
	assert(node != NULL);
	assert(tree->node_count > 0);
#endif
	if (tree->node_count <= 0)
		return;

#ifdef DEBUG
	struct fly_rb_node *__debug_free_node;
	enum __fly_rbtree_debug_type type;
	int ret;
	__fly_rbtree_debug(tree, __FLY_RBTREE_DEBUG_DELETE);

	if (!fly_rb_node_is_root(tree, node)){
		ret = __fly_rbtree_node_in_tree(tree->root->node, node);
		assert(ret == FLY_RBTREE_NODE_FOUND);
	}
	if (tree->node_count > 0)
		__fly_rbtree_node_in_node_data(tree->root->node);
#endif
	if (fly_rb_no_child(node)){
		/* release resource of rb */
		if (node->node_data != NULL)
			*node->node_data = NULL;

		if (fly_rb_node_is_root(tree, node)){
#ifdef DEBUG
			printf("__FLY_RBTREE_DEBUG_DELETE_ROOT\n");
			fflush(stdout);
#endif
			return fly_rb_root_release(tree);
		}else{
			if (fly_rb_node_is_left(node))
				node->parent->c_left = nil_node_ptr;
			else
				node->parent->c_right = nil_node_ptr;
		}
		if (fly_is_black(node->color))
			__fly_rb_delete_rebalance(tree, nil_node_ptr, node->parent);

		tree->node_count--;
#ifdef DEBUG
		__debug_free_node = node;
		if (tree->node_count > 0)
			__fly_rbtree_node_in_node_data(tree->root->node);
#endif
		fly_free(node);
#ifdef DEBUG
		type = __FLY_RBTREE_DEBUG_NOCHILD;
#endif
	}else if (node->c_right == nil_node_ptr){
		struct fly_rb_node *target;

		target = node->c_left;
		if (fly_rb_node_is_root(tree, node)){
			fly_rb_root(tree, target);
		}else{
			if (fly_rb_node_is_left(node))
				node->parent->c_left = target;
			else
				node->parent->c_right = target;

			target->parent = node->parent;
		}
		if (fly_is_black(node->color))
			__fly_rb_delete_rebalance(tree, target, target->parent);

		/* release resource of rb */
		if (node->node_data != NULL)
			*node->node_data = NULL;
		tree->node_count--;
#ifdef DEBUG
		__debug_free_node = node;
		if (tree->node_count > 0)
			__fly_rbtree_node_in_node_data(tree->root->node);
#endif
		fly_free(node);
#ifdef DEBUG
		type = __FLY_RBTREE_DEBUG_NORIGHT;
#endif
	}else{
		struct fly_rb_node *__p, *__m, *__mrc;

		__m = fly_rb_min_from_node(node);
		__p = __m->parent;

		if (node->node_data)
			*node->node_data = NULL;

		node->data = __m->data;
		node->key = __m->key;
		node->node_data = __m->node_data;

		if (__m->node_data)
			*__m->node_data = node;
		/* __m have one or zero child. */
		__mrc = __m->c_right;

		if (__p->c_left == __m)
			__p->c_left = __mrc;
		else
			__p->c_right = __mrc;
		fly_rb_parent(__mrc, __p);

		if (fly_is_black(__m->color)){
			if (__mrc == nil_node_ptr)
				__fly_rb_delete_rebalance(tree, nil_node_ptr, __p);
			else
				__fly_rb_delete_rebalance(tree, __mrc, __p);
		}
		tree->node_count--;
#ifdef DEBUG
		__debug_free_node = __m;
		if (tree->node_count > 0)
			__fly_rbtree_node_in_node_data(tree->root->node);
#endif
		/* release resource of rb */
		fly_free(__m);
#ifdef DEBUG
		type = __FLY_RBTREE_DEBUG_TWOCHILD;
#endif
	}
#ifdef DEBUG
	if (tree->node_count > 0)
		__fly_rbtree_free_node_in_node_data(tree->root->node, __debug_free_node, type);
	__fly_rbtree_debug(tree, type);
#endif
	return;
}

/*
 *	Red Black Tree Rules.
 *	1. Node have red or black color.
 *	2. Root node is black.
 *	3. All leaf node is black.
 *	4. Child of red node is black node.
 *	5. Every path from a given node to any of its descendant leaf nil nodes goed through the same number of black nodes.
 */
#ifdef DEBUG
#include <stdio.h>
void __fly_rbtree_debug(fly_rb_tree_t *tree, enum __fly_rbtree_debug_type type __unused)
{
	fly_rb_node_t *node;
	int black_count=0;
	if (tree->node_count == 0)
		return;

#ifdef DEBUG
	switch(type){
	case __FLY_RBTREE_DEBUG_DELETE:
		printf("__FLY_RBTREE_DEBUG_DELETE\n");
		fflush(stdout);
		break;
	case __FLY_RBTREE_DEBUG_INSERT:
		printf("__FLY_RBTREE_DEBUG_INSERT\n");
		fflush(stdout);
		break;
	case __FLY_RBTREE_DEBUG_NOCHILD:
		printf("__FLY_RBTREE_DEBUG_NOCHILD\n");
		fflush(stdout);
		break;
	case __FLY_RBTREE_DEBUG_NORIGHT:
		printf("__FLY_RBTREE_DEBUG_NORIGHT\n");
		fflush(stdout);
		break;
	case __FLY_RBTREE_DEBUG_TWOCHILD:
		printf("__FLY_RBTREE_DEBUG_TWOCHILD\n");
		fflush(stdout);
		break;
	default:
		break;
	}
#endif
	node = tree->root->node;

	/* Rule.2 */
	if (fly_rb_node_is_root(tree, node))
		assert(fly_is_black(node->color));

	__fly_rbtree_node_debug(tree, node, &black_count);
}

void __fly_rbtree_node_debug(fly_rb_tree_t *tree, fly_rb_node_t *node, int *black_count)
{
	int left_black_count=*black_count;
	int right_black_count=*black_count;

	/* Rule.1 */
	assert(fly_is_black(node->color) || fly_is_red(node->color));
	if (node->c_left == nil_node_ptr){
		/* Rule.3 */
		assert(fly_is_black(node->c_left->color));
		left_black_count++;
	}
	if (fly_is_red(node->color))
		/* Rule.4 */
		assert(fly_is_black(node->c_left->color));

	if (node->c_right == nil_node_ptr){
		/* Rule.3 */
		assert(fly_is_black(node->c_right->color));
		right_black_count++;
	}
	if (fly_is_red(node->color))
		/* Rule.4 */
		assert(fly_is_black(node->c_right->color));

	if (node->c_left != nil_node_ptr)
		__fly_rbtree_node_debug(tree, node->c_left, &left_black_count);

	if (node->c_right != nil_node_ptr)
		__fly_rbtree_node_debug(tree, node->c_right, &right_black_count);

	/* Rule.5 */
	assert(left_black_count == right_black_count);

	*black_count = left_black_count;
	if (fly_is_black(node->color))
		(*black_count)++;

	/* invalid access check */
	assert(nil_node.parent == nil_node_ptr);
	assert(nil_node.c_left == nil_node_ptr);
	assert(nil_node.c_right == nil_node_ptr);

	assert(tree->root->node->parent == nil_node_ptr);
	return;
}

int __fly_rbtree_node_in_tree(fly_rb_node_t *node, fly_rb_node_t *target)
{
	if (node->c_left != nil_node_ptr){
		if (node->c_left == target)
			return FLY_RBTREE_NODE_FOUND;
		else{
			if (__fly_rbtree_node_in_tree(node->c_left, target) == \
					FLY_RBTREE_NODE_FOUND)
				return FLY_RBTREE_NODE_FOUND;
		}
	}

	if (node->c_right != nil_node_ptr){
		if (node->c_right == target)
			return FLY_RBTREE_NODE_FOUND;
		else
			if (__fly_rbtree_node_in_tree(node->c_right, target) == \
					FLY_RBTREE_NODE_FOUND)
				return FLY_RBTREE_NODE_FOUND;
	}

	return FLY_RBTREE_NODE_NOTFOUND;
}

void __fly_rbtree_node_in_node_data(fly_rb_node_t *node)
{
	if (node->node_data != NULL)
		assert(*node->node_data == node);
	if (node->c_left != nil_node_ptr)
		__fly_rbtree_node_in_node_data(node->c_left);

	if (node->c_right != nil_node_ptr)
		__fly_rbtree_node_in_node_data(node->c_right);
}

void __fly_rbtree_free_node_in_node_data(fly_rb_node_t *node, fly_rb_node_t *freed, enum __fly_rbtree_debug_type type __unused)
{
	if (node->node_data != NULL)
		assert(*node->node_data != freed);
	if (node->c_left != nil_node_ptr)
		__fly_rbtree_free_node_in_node_data(node->c_left, freed, type);

	if (node->c_right != nil_node_ptr)
		__fly_rbtree_free_node_in_node_data(node->c_right, freed, type);
}

#endif
