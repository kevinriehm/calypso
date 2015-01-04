#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

#include "cell.h"
#include "env.h"
#include "htable.h"
#include "mem.h"

env_t *env_cons(env_t *parent) {
	env_t *env;

	env = mem_alloc(sizeof *env);
	assert(env);
	env->parent = parent;
	env->tab = htable_cons(0);

	return env;
}

env_t *env_parent(env_t *env) {
	return env ? env->parent : NULL;
}

bool env_get(env_t *env, string_t *sym, cell_t **val) {
	bool exists;
	hvalue_t hval;

	assert(env && sym);

	do exists = htable_lookup(env->tab,&sym,sizeof sym,&hval);
	while(!exists && (env = env->parent));

	if(exists && val)
		*val = hval.p;

	return exists;
}

void env_set(env_t *env, string_t *sym, cell_t *val, bool local) {
	bool exists;
	env_t *localenv;

	assert(env && sym);

	if(!local) {
		localenv = env;

		do exists = htable_lookup(env->tab,&sym,sizeof sym,NULL);
		while(!exists && (env = env->parent));

		if(!exists)
			env = localenv;
	}

	htable_insert(env->tab,&sym,sizeof sym,(hvalue_t) {
		.type = GC_TYPE(cell_t),
		.p = val
	});
}

