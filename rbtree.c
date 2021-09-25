#include "rbtree.h"

#define nil_node_ptr		&nil_node
static struct fly_rb_node nil_node = {
	.c_right = nil_node_ptr,
	.c_left = nil_node_ptr,
	.parent = nil_node_ptr,
	.parent_color = FLY_RB_UNKNOWN,
	.color = FLY_RB_BLACK,
	.data = NULL,
};

static inline void fly_rb_parent_color_update(struct fly_rb_node *node, fly_rb_color_t parent_color);
static inline void fly_rb_subst(struct fly_rb_node **dist, struct fly_rb_node **src);
static inline bool fly_rb_no_child(struct fly_rb_node *node);
static inline bool fly_rb_all_child(struct fly_rb_node *node);
static inline bool fly_rb_part_child(struct fly_rb_node *node);

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

__fly_static void __fly_rb_tree_release(fly_rb_node_t *left, fly_rb_node_t *right)
{
	if (left != nil_node_ptr){
		__fly_rb_tree_release(left->c_left, left->c_right);
		fly_free(left);
	}
	if (right != nil_node_ptr){
		__fly_rb_tree_release(right->c_left, right->c_right);
		fly_free(right);
	}
}

void fly_rb_tree_release(struct fly_rb_tree *tree)
{
	fly_rb_node_t *__n;
	/* all node was released, end */
	if (tree->node_count == 0)
		return;

	__n = tree->root->node;
	__fly_rb_tree_release(__n->c_left, __n->c_right);
	fly_free(__n);
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

    r->node = node;
	tree->root = r;
    return r;
}

static inline bool fly_rb_node_is_root(struct fly_rb_tree *tree, struct fly_rb_node *node)
{
    return tree->root->node == node ? true : false;
}

static inline void fly_rb_root(struct fly_rb_tree *tree, struct fly_rb_node *node)
{
	tree->root->node = node;
	node->parent = nil_node_ptr;
	fly_rb_parent_color_update(node, FLY_RB_UNKNOWN);
}

static inline bool fly_rb_parent_is_red(struct fly_rb_node *node)
{
    return node->parent_color & FLY_RB_RED ? true : false;
}

static inline bool fly_rb_parent_is_black(struct fly_rb_node *node)
{
    return node->parent_color == FLY_RB_BLACK? true : false;
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
    if (__g == nil_node_ptr)
        return nil_node_ptr;
    else if (__g->c_left == node->parent)
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
	fly_rb_parent_color_update(node, parent->color);
}

static void fly_rb_rotate_left(struct fly_rb_node *node, struct fly_rb_tree *tree)
{
    struct fly_rb_node **__n, *right;

    right = node->c_right;

	fly_rb_subst(&node->c_right, &right->c_left);
	if (node->c_right != nil_node_ptr)
		fly_rb_node_parent(right->c_left, node);

	fly_rb_subst(&right->c_left, &node);
    if (!fly_rb_node_is_root(tree, node)){
        if (fly_rb_node_is_left(node))
            __n = &node->parent->c_left;
        else
            __n = &node->parent->c_right;
        fly_rb_subst(__n, &right),
        fly_rb_node_parent(*__n, node->parent);
    }else
		fly_rb_root(tree, right);

    fly_rb_node_parent(node, right);
}

static void fly_rb_rotate_right(struct fly_rb_node *node, struct fly_rb_tree *tree)
{
    struct fly_rb_node **__n, *left;

    left = node->c_left;

	fly_rb_subst(&node->c_left, &left->c_right);
	if (node->c_left != nil_node_ptr)
        fly_rb_node_parent(left->c_right, node);

	fly_rb_subst(&left->c_right, &node);
    if (!fly_rb_node_is_root(tree, node)){
        if (fly_rb_node_is_left(node))
            __n = &node->parent->c_left;
        else
            __n = &node->parent->c_right;
		fly_rb_subst(__n, &left);
        fly_rb_node_parent(*__n, node->parent);
    }else
		fly_rb_root(tree, left);

    fly_rb_node_parent(node, left);
}

static inline void fly_rb_subst(struct fly_rb_node **dist, struct fly_rb_node **src)
{
	fly_rb_parent_color_update(*src, (*dist)->parent_color);
	*dist = *src;
}

static inline void fly_rb_parent_color_update(struct fly_rb_node *node, fly_rb_color_t parent_color)
{
    /* nil node color must not change */
    if (node == nil_node_ptr)   return;
    node->parent_color = parent_color;
}

static inline void fly_rb_color_update(struct fly_rb_node *node, fly_rb_color_t color)
{
    /* nil node color must not change */
    if (node == nil_node_ptr)   return;
    node->color = color;
	if (node->c_left != nil_node_ptr)
		fly_rb_parent_color_update(node->c_left, color);
	if (node->c_right != nil_node_ptr)
		fly_rb_parent_color_update(node->c_right, color);
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
	fly_rb_parent_color_update(node, FLY_RB_UNKNOWN);

	return node;
}

__fly_static int __fly_rb_node_from_key(struct fly_rb_tree *tree, struct fly_rb_node *node, void *key)
{
	return tree->cmp(node->key, key);
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

    if (tree->root == NULL){
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
                node->parent = __n;
                fly_rb_parent_color_update(node, __n->color);
                __n->c_left = node;
                break;
            }else
                __n = __n->c_left;
			continue;
        /* go right */
		case FLY_RB_CMP_BIG:
            if (__n->c_right == nil_node_ptr){
                node->parent = __n;
                fly_rb_parent_color_update(node, __n->color);
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

    while((__p=node->parent) != nil_node_ptr && fly_is_red(__p->color)){
        __g = __p->parent;
        if (fly_rb_node_is_left(__p)){
            {
                __u = fly_rb_get_uncle(node);
                if (__u != nil_node_ptr && fly_is_red(__u->color))
                {
                    fly_rb_color_update(__u, FLY_RB_BLACK);
                    fly_rb_color_update(__p, FLY_RB_BLACK);
                    fly_rb_color_update(__g, FLY_RB_RED);
                    node = __g;
                    continue;
                }
            }

            if (fly_rb_node_is_right(node))
            {
                struct fly_rb_node *tmp;
                fly_rb_rotate_left(__p, tree);

                tmp = __p;
                __p = node;
                node = tmp;
            }

           fly_rb_color_update(__p, FLY_RB_BLACK);
           fly_rb_color_update(__g, FLY_RB_RED);
           fly_rb_rotate_right(__g, tree);
        }else{
            {
                __u = fly_rb_get_uncle(node);
                if (__u != nil_node_ptr && fly_is_red(__u->color))
                {
                    fly_rb_color_update(__u, FLY_RB_BLACK);
                    fly_rb_color_update(__p, FLY_RB_BLACK);
                    fly_rb_color_update(__g, FLY_RB_RED);
                    node = __g;
                    continue;
                }
            }

            if (fly_rb_node_is_left(node))
            {
                struct fly_rb_node *tmp;
                fly_rb_rotate_right(__p, tree);

                tmp = __p;
                __p = node;
                node = tmp;
            }

            fly_rb_color_update(__p, FLY_RB_BLACK);
            fly_rb_color_update(__g, FLY_RB_RED);
            fly_rb_rotate_left(__g, tree);
        }
    }
    tree->node_count++;
    return;
}

static inline void fly_rb_update_value(struct fly_rb_node *dist, struct fly_rb_node *src)
{
	dist->key = src->key;
	dist->data = src->data;
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

__fly_static void __fly_rb_delete_rebalance(struct fly_rb_tree *tree, struct fly_rb_node *node)//, struct fly_rb_node *__m, struct fly_rb_node *__mrc)
{
	struct fly_rb_node *__p, *__s;

	__p = node->parent;
	__s = fly_rb_sibling(node);

	/* left child and right child is nil */
	if (fly_rb_no_child(node)){
		return;
	/* right child is not nil */
	}else{
		/* node is red */
		if (fly_is_red(node->color))
			return;
		else{
			/* sibling is red */
			if (fly_is_red(__s->color)){
				if (fly_rb_node_is_left(node))
					fly_rb_rotate_left(__p, tree);
				else
					fly_rb_rotate_right(__p, tree);

				fly_rb_reverse_color(__p);
				fly_rb_reverse_color(__s);

				__s = fly_rb_sibling(node);
				return __fly_rb_delete_rebalance(tree, node);
			/* sibling is black */
			}else{
				if (fly_is_black(__p->color) && fly_is_black(__s->c_left->color) && fly_is_black(__s->c_right->color)){
					fly_rb_color_update(__s, FLY_RB_RED);
					return __fly_rb_delete_rebalance(tree, __p);
				} else if (fly_is_red(__p->color) && fly_is_black(__s->c_left->color) && fly_is_black(__s->c_right->color)){
					fly_rb_color_update(__p, FLY_RB_BLACK);
					fly_rb_color_update(__s, FLY_RB_RED);
					return;
				} else if (fly_is_red(__s->c_right->color) && fly_rb_node_is_left(node)){
					fly_rb_color_t __scolor = __s->c_right->color;
					fly_rb_rotate_left(__p, tree);
					fly_rb_color_update(__s->c_right, FLY_RB_BLACK);

					fly_rb_color_update(__s->c_right, __p->color);
					fly_rb_color_update(__p, __scolor);

					return __fly_rb_delete_rebalance(tree, __p);
				} else if (fly_is_red(__s->c_left->color) && fly_rb_node_is_right(node)){
					fly_rb_color_t __scolor = __s->c_left->color;
					fly_rb_rotate_right(__p, tree);
					fly_rb_color_update(__s->c_left, FLY_RB_BLACK);

					fly_rb_color_update(__s->c_left, __p->color);
					fly_rb_color_update(__p, __scolor);
					return;
				}
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
    struct fly_rb_node *__m, *__mrc=NULL, *__p;

    __m = node->c_right;
    __p = node->parent;

    /* left child and right child is nil */
	if (fly_rb_no_child(node)){
        if (fly_rb_node_is_left(node))
            node->parent->c_left = nil_node_ptr;
		else
            node->parent->c_right = nil_node_ptr;
        tree->node_count--;
		/* release resource of rb */
		fly_free(node);
        return;
    /* have one child */
	} else if (fly_rb_part_child(node)){
		struct fly_rb_node *target;
		fly_rb_color_t node_color, subst_color;

		target = node->c_left != nil_node_ptr ? node->c_left : node->c_right;

		node_color = node->color;
		subst_color = target->color;
		fly_rb_subst(&node, &target);

		/* release resource of rb */
        tree->node_count--;
		fly_free(node);

		if (fly_is_red(node_color))
			return;
		if (fly_is_black(node_color) && fly_is_red(subst_color)){
			fly_rb_color_update(target, FLY_RB_BLACK);
			return;
		}else
			return __fly_rb_delete_rebalance(tree, target);
	/* have two children */
	}else{
        while(__m->c_left != nil_node_ptr)
            __m = __m->c_left;

        fly_rb_update_value(node, __m);
        __mrc = __m->c_right;
		fly_free(__m);
		if (fly_rb_node_is_left(__m))
			fly_rb_subst(&__p->c_left, &__mrc);
		else
			fly_rb_subst(&__p->c_right, &__mrc);
		tree->node_count--;
		return __fly_rb_delete_rebalance(tree, node);
	}
}

