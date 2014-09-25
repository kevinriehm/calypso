#include <assert.h>
#include <stdio.h>

#include "cell.h"
#include "env.h"
#include "util.h"

extern FILE *yyin;
extern cell_t *parseroot;

int yyparse(void);

bool readf(FILE *f, cell_t **cell) {
	int err;

	yyin = f;

	err = yyparse();

	if(cell)
		*cell = parseroot;

	return !err;
}

cell_t *eval(env_t *env, cell_t *sexp) {
	cell_t *op;

	if(!sexp)
		return sexp;

	switch(sexp->car.type) {
	case VAL_NIL:
	case VAL_I64:
	case VAL_DBL:
	case VAL_CHR:
	case VAL_STR:
	case VAL_FCN:
		return sexp;

	case VAL_SYM:
		return env_get(env,sexp->cdr.str,&sexp) ? sexp : NULL;

	default:
		assert(sexp->car.type > NUM_VAL_TYPES);

		op = eval(env,sexp->car.p);
		assert(op && op->car.type == VAL_FCN);
		return op->cdr.func(env,sexp->cdr.p);
	}

	error("unhandled s-expression of type %i",sexp->car.type);

	return NULL;
}

void print(cell_t *sexp) {
	if(!sexp) {
		printf("nil");
		return;
	}

	switch(sexp->car.type) {
	case VAL_NIL: printf("nil");                  break;
	case VAL_SYM: printf("%s",sexp->cdr.str);     break;
	case VAL_I64: printf("%lli",sexp->cdr.i64);   break;
	case VAL_DBL: printf("%f",sexp->cdr.dbl);     break;
	case VAL_CHR: printf("'%c'",sexp->cdr.chr);   break;
	case VAL_STR: printf("\"%s\"",sexp->cdr.str); break;
	case VAL_FCN: printf("<%p>",sexp->cdr.dbl);   break;

	default:
		assert(sexp->car.type > NUM_VAL_TYPES);

		putchar('(');
		do {
			print(sexp->car.p);
			if(sexp->cdr.p) putchar(' ');
		} while(sexp = sexp->cdr.p);
		putchar(')');
		break;
	}
}

void run_file(env_t *env, FILE *in) {
	cell_t *sexp;

	while(readf(in,&sexp)) {
		print(eval(env,sexp));
		putchar('\n');
	}
}

