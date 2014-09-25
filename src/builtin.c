#include <assert.h>
#include <stdint.h>

#include "cell.h"
#include "env.h"
#include "repl.h"

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
		case VAL_DBL: xdbl = xi64 = x->cdr.dbl; break;
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

void builtin_init(env_t *env) {
	env_set(env,"+",cell_cons_t(VAL_FCN,add),true);
}

