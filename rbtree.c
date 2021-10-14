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
	}

	if (tree->root){
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
	return r;
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
	fly_rb_color_update(node, FLY_RB_RED);

	return node;
}

__fly_static int __fly_rb_node_from_key(struct fly_rb_tree *tree, struct fly_rb_node *node, void *key)
{
	return tree->cmp(key, node->key);
}

void *fly_rb_node_data_from_key(struct fly_rb_tree *tree, void *key)
{
	fly_rb_node_t *__n;

	__n = fly_rb_node_from_key(tree, key);
	if (fly_unlikely_null(__n))
		return NULL;
	else
		return __n->data;
}

fly_rb_node_t *fly_rb_node_from_key(struct fly_rb_tree *tree, void *key)
{
	struct fly_rb_node *__n;
	if (!tree->cmp)
		return NULL;

	__n = tree->root->node;
	while(__n != nil_node_ptr){
		switch(__fly_rb_node_from_key(tree, __n, key)){
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

fly_rb_node_t *fly_rb_tree_insert(struct fly_rb_tree *tree, void *data, void *key)
{
	fly_rb_node_t *node;

	node = __fly_node_init(data, key);
	if (fly_unlikely_null(node))
		return NULL;
	fly_rb_tree_insert_node(tree, node);
	return node;
}

void fly_rb_tree_insert_node(struct fly_rb_tree *tree, struct fly_rb_node *node)
{
    struct fly_rb_node *__n, *__p, *__g, *__u;

    node->color = FLY_RB_RED;
    node->c_right = nil_node_ptr;
    node->c_left = nil_node_ptr;
    node->parent = nil_node_ptr;

    if (tree->node_count == 0){
        tree->root = fly_rb_root_init(tree, node);
        tree->node_count++;
        return;
    }

    __n = tree->root->node;
    while(true){
		switch(tree->cmp(node->key, __n->key)){
		case FLY_RB_CMP_EQUAL:
            return;
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
    return;
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
			 *	   P(B)
			 *	   / \
			 *	N(B)  S(R)
			 *		  / \
			 *		Sl   Sr
			 */
			if (__p->c_left == node){
				fly_rb_rotate_left(__p, tree);
			/*
			 *		   P(B)
			 *		   / \
			 *		S(B)  N(R)
			 *		/ \
			 *	  Sl   Sr
			 */
			}else if (__p->c_right == node){
				fly_rb_rotate_right(__p, tree);
			}else
				FLY_NOT_COME_HERE

			/* parent become red */
			fly_rb_reverse_color(__p);
			/* sibling become black */
			fly_rb_reverse_color(__s);

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
			} else if (fly_is_red(__s->c_left->color) && fly_is_black(__s->c_right->color) && fly_rb_node_is_left(node)){
				fly_rb_rotate_right(__s, tree);
				fly_rb_color_update(__s, FLY_RB_RED);
				fly_rb_color_update(__s->c_left, FLY_RB_BLACK);

				goto recursion;
			/*
			 *		   P(B or B)
			 *		   / \
			 *		S(B)  N(B)
			 *		/  \
			 *	Sl(B)   Sr(R)
			 */
			} else if (fly_is_black(__s->c_left->color) && fly_is_red(__s->c_right->color) && fly_rb_node_is_right(node)){
				fly_rb_rotate_left(__s, tree);
				fly_rb_color_update(__s, FLY_RB_RED);
				fly_rb_color_update(__s->c_right, FLY_RB_BLACK);

				goto recursion;
			/*
			 *		   P(B or B)
			 *		   / \
			 *		N(B)  S(B)
			 *			 /	  \
			 *	 Sl(B or R)  Sr(R)
			 */
			} else if (fly_is_red(__s->c_right->color) && fly_rb_node_is_left(node)){
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
			} else if (fly_is_red(__s->c_left->color) && fly_rb_node_is_right(node)){
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

void fly_rb_delete(struct fly_rb_tree *tree, struct fly_rb_node *node)
{
    struct fly_rb_node *__m, *__mrc=NULL, *__p __unused, *target;

	if (fly_unlikely_null(node))
		return;

    __p = node->parent;

	if (tree->node_count <= 0)
		return;

	if (node->c_left == nil_node_ptr){
		target = node->c_right;
	}else if (node->c_right == nil_node_ptr){
		target->c_right = c_left;
	if (fly_rb_no_child(node)){
		if (fly_rb_node_is_root(tree, node)){
			return fly_rb_root_release(tree);
		}else{
			if (fly_rb_node_is_left(node))
				node->parent->c_left = nil_node_ptr;
			else
				node->parent->c_right = nil_node_ptr;
		}
		/* release resource of rb */
		if (fly_is_black(node->color))
			__fly_rb_delete_rebalance(tree, nil_node_ptr, node->parent);
        tree->node_count--;
		fly_free(node);
        return;
    /* have one child */
	} else if (fly_rb_part_child(node)){

		target = (node->c_left!=nil_node_ptr) ? node->c_left : node->c_right;

		if (fly_rb_node_is_root(tree, node)){
			fly_rb_root(tree, target);
		}else{
			if (fly_rb_node_is_left(node))
				node->parent->c_left = target;
			else
				node->parent->c_right = target;

			target->parent = node->parent;
		}

		/* release resource of rb */

		if (fly_is_black(node->color))
			__fly_rb_delete_rebalance(tree, target, node->parent);

        tree->node_count--;
		fly_free(node);
		return;
	/* have two children */
	}else{
		struct fly_rb_node *__p;
		__m = node->c_right; /* not nil_node */
        while(__m->c_left != nil_node_ptr)
            __m = __m->c_left;

		__p = __m->parent;
        fly_rb_swap(tree, node, __m);
        __mrc = node->c_right;
		node->parent->c_left = __mrc;
		if (__mrc != nil_node_ptr)
			__mrc->parent = node->parent;

		if (fly_is_black(node->color))
			__fly_rb_delete_rebalance(tree, __mrc, __p);

		tree->node_count--;
		fly_free(node);
		return;
	}
}

