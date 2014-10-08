#include <assert.h>
#include <setjmp.h>
#include <stdio.h>
#include <stdlib.h>

#include <unistd.h>

#include "cell.h"
#include "check.h"
#include "env.h"
#include "grammar.h"
#include "repl.h"
#include "token.h"
#include "util.h"

int lineno;
char *filename;
jmp_buf checkjmp;

void *ParseAlloc(void *(*)(size_t));
void ParseFree(void *, void (*)(void *));
void Parse(void *, int, token_value_t, cell_t **);
void ParseTrace(FILE *, char *);

bool readf(void *p, stream_t *s, cell_t **cell) {
	int tok;
	uint32_t level;
	cell_t sentinel;
	token_value_t tokval;

	level = 0;

	*cell = &sentinel;

	if(stream_interactive(s)) {
		printf("> ");
		fflush(stdout);
	}

	do {
		tok = token_next(s,&tokval);
		Parse(p,tok,tokval,cell);

		if(tok == LPAREN) level++;
		if(tok == RPAREN) level--;

		if(!level && (tok == RPAREN || tok == SYMBOL))
			Parse(p,0,tokval,cell);
	} while(*cell == &sentinel && tok > 0);

	return *cell != &sentinel;
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
	check(!lambarg,"too few arguments to lambda expression");

	// Evaluate the expressions
	val = NULL;
	for(lambexpr = lamb->body; lambexpr; lambexpr = lambexpr->cdr.p)
		val = eval(lambenv,lambexpr->car.p);

	env_free(lambenv);

	return val;
}

static void bind_macro_args(env_t *env, cell_t *template, cell_t *args) {
	for(; args && template;
		args = args->cdr.p, template = template->cdr.p) {
		if(cell_is_list(template->car.p) && cell_is_list(args->car.p))
			bind_macro_args(env,template->car.p,args->car.p);
		else if(template->car.p->car.type == VAL_SYM)
			env_set(env,template->car.p->cdr.str,args->car.p,true);
		else check(false,"mal-formed macro arguments");
	}

	check(!template,"too few arguments to macro expression");
}

static cell_t *eval_macro(env_t *env, lambda_t *mac, cell_t *args) {
	env_t *macenv;
	cell_t *expr, *val;

	macenv = env_cons(mac->env);

	// Bind the arguments
	bind_macro_args(macenv,mac->args,args);

	// Expand it
	for(expr = mac->body, val = NULL; expr; expr = expr->cdr.p)
		val = eval(macenv,expr->car.p);

	// Then evaluate the expansion
	val = eval(env,val);

	env_free(macenv);

	return val;
}

cell_t *eval(env_t *env, cell_t *sexp) {
	env_t *lambenv;
	lambda_t *lamb;
	cell_t *arg, *body, *op;

	if(!sexp)
		return sexp;

	switch(sexp->car.type) {
	case VAL_I64:
	case VAL_DBL:
	case VAL_CHR:
	case VAL_STR:
	case VAL_FCN:
		return sexp;

	case VAL_SYM:
		return env_get(env,sexp->cdr.str,&sexp) ? sexp : NULL;

	case VAL_NIL:
	default:
		assert(cell_is_list(sexp));

		op = eval(env,sexp->car.p);
		check(op && (op->car.type == VAL_FCN
			|| op->car.type == VAL_LBA),
			"operator must be a function");
		if(op->car.type == VAL_FCN)
			return op->cdr.fcn(env,sexp->cdr.p);
		else if(op->car.type == VAL_LBA) {
			return op->cdr.lba->ismacro
				? eval_macro(env,op->cdr.lba,sexp->cdr.p)
				: eval_lambda(env,op->cdr.lba,sexp->cdr.p);
		}
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
	case VAL_SYM: printf("%s",sexp->cdr.str);     break;
	case VAL_I64: printf("%lli",sexp->cdr.i64);   break;
	case VAL_DBL: printf("%f",sexp->cdr.dbl);     break;
	case VAL_CHR: printf("'%c'",sexp->cdr.chr);   break;
	case VAL_STR: printf("\"%s\"",sexp->cdr.str); break;
	case VAL_FCN: printf("<%p>",sexp->cdr.dbl);   break;
	case VAL_LBA: printf("<%s>",sexp->cdr.lba->ismacro
		? "macro" : "lambda"); break;

	case VAL_NIL:
	default:
		assert(cell_is_list(sexp));

		putchar('(');
		do {
			if(cell_is_list(sexp)) {
				print(sexp->car.p);
				if(sexp->cdr.p)
					putchar(' ');
			} else {
				printf(" . ");
				print(sexp);
			}
		} while(sexp = sexp->cdr.p);
		putchar(')');
		break;
	}
}

void run_file(env_t *env, FILE *in) {
	void *p;
	stream_t *s;
	cell_t *sexp;
	bool interactive;

	// Set up the parser
	p = ParseAlloc(malloc);
	s = stream_cons_f(in);

	// Catch check failures (i.e., run-time errors)
	setjmp(checkjmp);

	while(true) {
		if(!readf(p,s,&sexp))
			break;

		sexp = eval(env,sexp);

		if(stream_interactive(s)) {
			print(sexp);
			putchar('\n');
		}
	}

	if(stream_interactive(s))
		putchar('\n');

	// Clean up
	stream_free(s);
	ParseFree(p,free);
}

