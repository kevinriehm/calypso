#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

#include "cell.h"
#include "env.h"
#include "htable.h"

struct env {
	uint32_t ref;

	struct env *parent;

	htable_t *tab;
};

env_t *env_cons(env_t *parent) {
	env_t *env;

	env = malloc(sizeof *env);
	assert(env);
	env->ref = 1;
	env->parent = parent;
	env->tab = htable_cons(0);

	assert(!parent || parent->ref++);

	return env;
}

void env_free(env_t *env) {
	if(!env || --env->ref)
		return;

	env_free(env->parent);

	htable_free(env->tab);
}

env_t *env_ref(env_t *env) {
	if(env) env->ref++;

	return env;
}

bool env_get(env_t *env, char *name, cell_t **val) {
	bool exists;
	hvalue_t hval;

	assert(env && name);

	do exists = htable_lookup(env->tab,name,&hval);
	while(!exists && (env = env->parent));

	if(exists && val)
		*val = hval.p;

	return exists;
}

void env_set(env_t *env, char *name, cell_t *val, bool local) {
	bool exists;
	env_t *localenv;

	assert(env && name);

	if(!local) {
		localenv = env;

		do exists = htable_lookup(env->tab,name,NULL);
		while(!exists && (env = env->parent));

		if(!exists)
			env = localenv;
	}

	htable_insert(env->tab,name,(hvalue_t) { .p = val });
}

