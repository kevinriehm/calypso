typedef struct rb_tree rb_tree_t;

rb_tree_t *rb_cons();
void rb_insert(rb_tree_t *, char *, void *);
void *rb_search(rb_tree_t *, char *);
void rb_delete(rb_tree_t *, char *);

