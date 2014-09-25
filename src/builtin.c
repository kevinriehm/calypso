#include <assert.h>
#include <stdint.h>

#include "cell.h"
#include "env.h"
#include "repl.h"
#include "util.h"

static cell_t *tsym;

static cell_t *atom(env_t *env, cell_t *args) {
	assert(args && !args->cdr.p);

	args = eval(env,args->car.p);

	return !args || args->car.type < NUM_VAL_TYPES ? tsym : NULL;
}

static cell_t *car(env_t *env, cell_t *args) {
	assert(args && !args->cdr.p);
	args = eval(env,args->car.p);

	assert(args && args->car.type > NUM_VAL_TYPES);

	return args->car.p;
}

static cell_t *cdr(env_t *env, cell_t *args) {
	assert(args && !args->cdr.p);
	args = eval(env,args->car.p);

	assert(args && args->car.type > NUM_VAL_TYPES);

	return args->cdr.p;
}

static cell_t *cond(env_t *env, cell_t *args) {
	cell_t *pair;

	for(; args; args = args->cdr.p) {
		pair = args->car.p;
		assert(pair && pair->cdr.p && !pair->cdr.p->cdr.p);

		if(eval(env,pair->car.p))
			return eval(env,pair->cdr.p->car.p);
	}

	return NULL;
}

static cell_t *cons(env_t *env, cell_t *args) {
	cell_t *a, *b;

	assert(args);
	a = eval(env,args->car.p);

	args = args->cdr.p;
	assert(args);
	b = eval(env,args->car.p);
	assert(!b || b->car.p > NUM_VAL_TYPES);

	assert(!args->cdr.p);

	return cell_cons(a,b);
}

static cell_t *eq(env_t *env, cell_t *args) {
	cell_t *a, *b;

	assert(args);
	a = eval(env,args->car.p);

	args = args->cdr.p;
	assert(args);
	b = eval(env,args->car.p);

	assert(!args->cdr.p);

	if(!a || !b)
		return a || b ? NULL : tsym;

	if(a && b && a->car.type != b->car.type)
		return NULL;

	switch(a->car.type) {
	case VAL_NIL: return tsym;
	case VAL_SYM: return strcmp(a->cdr.str,b->cdr.str) == 0 ? tsym : NULL;
	case VAL_I64: return a->cdr.i64 == b->cdr.i64 ? tsym : NULL;
	case VAL_DBL: return a->cdr.dbl == b->cdr.dbl ? tsym : NULL;
	case VAL_CHR: return a->cdr.chr == b->cdr.chr ? tsym : NULL;
	case VAL_FCN: return a->cdr.func == b->cdr.func ? tsym : NULL;
	case VAL_STR: return strcmp(a->cdr.str,b->cdr.str) == 0 ? tsym : NULL;

	default:
		assert(a->car.type < NUM_VAL_TYPES);
		error("unhandled value in eq, type %i",a->car.type);
		return NULL;
	}
}

static cell_t *quote(env_t *env, cell_t *args) {
	assert(args && !args->cdr.p);

	return args->car.p;
}

static cell_t *assign(env_t *env, cell_t *args) {
	cell_t *sym, *val;

	// The symbol to assign to
	assert(args && args->car.type > NUM_VAL_TYPES);
	sym = args->car.p;
	assert(sym && sym->car.type == VAL_SYM);

	// The value to assign
	args = args->cdr.p;
	assert(args && args->car.type > NUM_VAL_TYPES);
	val = eval(env,args->car.p);

	assert(!args->cdr.p);

	// Assign it!
	env_set(env,sym->cdr.str,val,false);

	return val;
}

static cell_t *add(env_t *env, cell_t *args) {
	cell_t *r, *x;
	double dbl, xdbl;
	int64_t i64, xi64;
	enum cell_type type;

	dbl = i64 = 0;
	type = VAL_NIL;
	for(; args; args = args->cdr.p) {
		x = eval(env,args->car.p);

		assert(x
			&& (x->car.type == VAL_I64 || x->car.type == VAL_DBL));

		switch(x->car.type) {
		case VAL_I64: xdbl = xi64 = x->cdr.i64; break;
		case VAL_DBL: xi64 = xdbl = x->cdr.dbl; break;
		}

add_x:
		switch(type) {
		case VAL_NIL: type = x->car.type; goto add_x;

		case VAL_I64: i64 += xi64; break;
		case VAL_DBL: dbl += xdbl; break;
		}
	}

	switch(type) {
	case VAL_NIL: return cell_cons_t(VAL_DBL,0.);
	case VAL_I64: return cell_cons_t(VAL_I64,i64);
	case VAL_DBL: return cell_cons_t(VAL_DBL,dbl);
	}
}

static cell_t *sub(env_t *env, cell_t *args) {
	cell_t *r, *x;
	double dbl, xdbl;
	int64_t i64, xi64;
	enum cell_type type;

	dbl = i64 = 0;
	type = VAL_NIL;
	for(; args; args = args->cdr.p) {
		x = eval(env,args->car.p);

		assert(x
			&& (x->car.type == VAL_I64 || x->car.type == VAL_DBL));

		switch(x->car.type) {
		case VAL_I64: xdbl = xi64 = x->cdr.i64; break;
		case VAL_DBL: xi64 = xdbl = x->cdr.dbl; break;
		}

sub_x:
		switch(type) {
		case VAL_NIL:
			type = x->car.type;

			if(args->cdr.p) {
				xdbl = -xdbl;
				xi64 = -xi64;
			}

			goto sub_x;

		case VAL_I64: i64 -= xi64; break;
		case VAL_DBL: dbl -= xdbl; break;
		}
	}

	switch(type) {
	case VAL_NIL: return cell_cons_t(VAL_DBL,0.);
	case VAL_I64: return cell_cons_t(VAL_I64,i64);
	case VAL_DBL: return cell_cons_t(VAL_DBL,dbl);
	}
}

void builtin_init(env_t *env) {
	static const struct {
		char *name;
		cell_t *(*func)(env_t *, cell_t *);
	} funcs[] = {
		{"atom", atom},
		{"car", car},
		{"cdr", cdr},
		{"cond", cond},
		{"cons", cons},
		{"eq", eq},
		{"quote", quote},
		{"=", assign},
		{"+", add},
		{"-", sub},
		{NULL, NULL}
	};

	// Canonical truth symbol
	tsym = cell_cons_t(VAL_SYM,"t");
	env_set(env,tsym->cdr.str,tsym,true);

	// Builtin operators
	for(int i = 0; funcs[i].name; i++)
		env_set(env,funcs[i].name,cell_cons_t(VAL_FCN,funcs[i].func),
			true);
}

