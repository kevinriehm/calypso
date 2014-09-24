#ifndef ENV_H
#define ENV_H

#include <stdbool.h>

typedef struct env env_t;

env_t *env_cons(env_t *);
void env_free(env_t *);

bool env_get(env_t *, char *, struct cell **);
void env_set(env_t *, char *, struct cell *, bool);

#endif

