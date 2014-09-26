#include <assert.h>
#include <setjmp.h>
#include <signal.h>
#include <stdio.h>

#include "cell.h"
#include "env.h"
#include "repl.h"
#include "util.h"

extern FILE *yyin;
extern cell_t *parseroot;

static jmp_buf abortjmp;

int yyparse(void);

bool readf(FILE *f, cell_t **cell) {
	int err;

	yyin = f;

	err = yyparse();

	if(cell)
		*cell = parseroot;

	return !err;
}

static cell_t *eval_lambda(env_t *env, lambda_t *lamb, cell_t *args) {
	env_t *lambenv;
	cell_t *lambarg, *lambexpr, *val;

	lambenv = env_cons(lamb->env);

	// Bind the arguments
	for(lambarg = lamb->args; args && lambarg;
		args = args->cdr.p, lambarg = lambarg->cdr.p)
		env_set(lambenv,lambarg->car.p->cdr.str,eval(env,args->car.p),
			true);
	assert(!lambarg);

	// Evaluate the expressions
	for(lambexpr = lamb->body; lambexpr; lambexpr = lambexpr->cdr.p)
		val = eval(lambenv,lambexpr->car.p);

	env_free(lambenv);

	return val;
}

cell_t *eval(env_t *env, cell_t *sexp) {
	env_t *lambenv;
	lambda_t *lamb;
	cell_t *arg, *body, *op;

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
		assert(op && (op->car.type == VAL_FCN
			|| op->car.type == VAL_LBA));
		if(op->car.type == VAL_FCN)
			return op->cdr.fcn(env,sexp->cdr.p);
		else if(op->car.type == VAL_LBA)
			return eval_lambda(env,op->cdr.lba,sexp->cdr.p);
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
	case VAL_LBA: printf("<lambda>");             break;

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

static void handle_abort(int signum) {
	// Sanity check
	if(signum != SIGABRT)
		raise(signum);

	// Be extremely descriptive and helpful
	error("caught run-time error");

	// Keep calm and carry on
	longjmp(abortjmp,1);
}

void run_file(env_t *env, FILE *in) {
	cell_t *sexp;
	void (*oldhandler)(int) ;

	// Catch assertion failures (i.e., run-time errors)
	if(setjmp(abortjmp) == 0) {
		oldhandler = signal(SIGABRT,handle_abort);
		assert(oldhandler != SIG_ERR);
	}

	while(readf(in,&sexp)) {
		print(eval(env,sexp));
		putchar('\n');
	}

	signal(SIGABRT,oldhandler);
}

