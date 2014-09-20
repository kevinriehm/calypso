#include <assert.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

typedef struct rb_node {
	bool red;

	char *key;
	void *val;

	struct rb_node *p;
	struct rb_node *a, *b;
} rb_node_t;

typedef struct rb_tree {
	rb_node_t *root;
} rb_tree_t;

static void rotate_left(rb_node_t *pivot) {
	rb_node_t *p;

	p = pivot->p;

	p->b = pivot->a;

	pivot->p = p->p;
	pivot->a = p;

	if(p->p) {
		if(p->p->a == p)
			p->p->a = pivot;
		else p->p->b = pivot;
	}

	p->p = pivot;
}

static void rotate_right(rb_node_t *pivot) {
	rb_node_t *p;

	p = pivot->p;

	p->a = pivot->b;

	pivot->p = p->p;
	pivot->b = p;

	if(p->p) {
		if(p->p->a == p)
			p->p->a = pivot;
		else p->p->b = pivot;
	}

	p->p = pivot;
}

rb_tree_t *rb_cons() {
	rb_tree_t *tree;

	tree = calloc(1,sizeof(rb_tree_t));
	tree->root = calloc(1,sizeof *tree->root);

	return tree;
}

void rb_insert(rb_tree_t *tree, char *key, void *val) {
	int r;
	rb_node_t **child, **p, *root, *t, *uncle;

	root = tree->root;

	// Normal insertion
	if(!root->key) {
		root->key = strdup(key);
		root->val = val;
	} else while(true) {
		r = strcmp(key,root->key);

		if(r == 0) {
			root->val = val;
			break;
		}

		if(r < 0) child = &root->a;
		else      child = &root->b;

		if(!*child) {
			*child = malloc(sizeof **child);

			(*child)->red = true;
			(*child)->key = strdup(key);
			(*child)->val = val;

			(*child)->p = root;
			(*child)->a = NULL;
			(*child)->b = NULL;

			root = *child;
			break;
		}

		root = *child;
	}

	// Re-balance the tree
rebalance:
	p = &root->p;

	if(*p == NULL) {
		root->red = false;
		return;
	}

	if(!(*p)->red) return;

	uncle = (*p)->p->a == root->p ? (*p)->p->b : (*p)->p->a;

	if(uncle && uncle->red) {
		(*p)->red = uncle->red = false;
		(*p)->p->red = true;

		root = (*p)->p;
		goto rebalance;
	}

	if(root == (*p)->b && *p == (*p)->p->a) {
		rotate_left(root);
		root = root->a;
	} else if(root == (*p)->a && *p == (*p)->p->b) {
		rotate_right(root);
		root = root->b;
	}

	root->p->red = false;
	root->p->p->red = true;

	if(root == root->p->a)
		rotate_right(root->p);
	else rotate_left(root->p);

	// Climb back up the tree
	while(tree->root->p) tree->root = tree->root->p;
}

void *rb_search(rb_tree_t *tree, char *key) {
	int r;
	rb_node_t *root;

	root = tree->root;

	while(root) {
		r = strcmp(key,root->key);

		if(r == 0) return root->val;

		if(r < 0) root = root->a;
		else      root = root->b;
	}

	return NULL;
}

void rb_delete(rb_tree_t *tree, char *key) {
	assert(("rb_delete() not implemented", false));
}

