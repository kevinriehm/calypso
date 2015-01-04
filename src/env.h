#ifndef ENV_H
#define ENV_H

#include <stdbool.h>

struct string;

typedef struct env {
	struct env *parent;

	struct htable *tab;
} env_t;

env_t *env_cons(env_t *);

env_t *env_parent(env_t *);

bool env_get(env_t *, struct string *, struct cell **);
void env_set(env_t *, struct string *, struct cell *, bool);

#endif

