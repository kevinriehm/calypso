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

static void bind_args(env_t *env, cell_t *template, cell_t *args, bool macro) {
	cell_t *head, **tail;

	for(; args && template; args = args->cdr, template = template->cdr) {
		// Skip nil in the template
		while(!template->car) {
			template = template->cdr;
			if(!template)
				break;
		}

		if(cell_type(template) == VAL_SYM) { // Var-args
			// Evaluate the args for normal lambdas
			for(head = NULL, tail = &head; !macro && args;
				args = args->cdr, tail = &(*tail)->cdr)
				*tail = cell_cons(eval(env,args->car),NULL);

			env_set(env,template->sym,macro ? args : head,true);
			break;
		} else if(cell_is_list(template->car)
			&& cell_is_list(args->car))
			bind_args(env,template->car,args->car,macro);
		else if(cell_type(template->car) == VAL_SYM)
			env_set(env,template->car->sym,
				macro ? args->car : eval(env,args->car),true);
		else check(false,"mal-formed macro arguments");
	}

	check(cell_is_atom(template) || !template,
		"too few arguments to macro expression");
}

cell_t *eval_lambda(env_t *env, lambda_t *lamb, cell_t *args) {
	env_t *lambenv;
	cell_t *lambarg, *lambexpr, *val;

	lambenv = env_cons(lamb->env);

	// Bind the arguments
	bind_args(lambenv,lamb->args,args,lamb->ismacro);

	// Evaluate the body
	val = NULL;
	for(lambexpr = lamb->body; lambexpr; lambexpr = lambexpr->cdr)
		val = eval(lambenv,lambexpr->car);

	env_free(lambenv);

	return val;
}

static cell_t *eval_macro(env_t *env, lambda_t *mac, cell_t *args) {
	return eval(env,eval_lambda(env,mac,args));
}

cell_t *eval(env_t *env, cell_t *sexp) {
	env_t *lambenv;
	lambda_t *lamb;
	cell_t *arg, *body, *op;

	if(!sexp)
		return sexp;

	switch(cell_type(sexp)) {
	case VAL_I64:
	case VAL_DBL:
	case VAL_CHR:
	case VAL_STR:
	case VAL_FCN:
		return sexp;

	case VAL_SYM:
		return env_get(env,sexp->sym,&sexp) ? sexp : NULL;

	case VAL_NIL:
	default:
		assert(cell_is_list(sexp));

		op = eval(env,sexp->car);
		check(op && (cell_type(op) == VAL_FCN
			|| cell_type(op) == VAL_LBA),
			"operator must be a function");
		if(cell_type(op) == VAL_FCN)
			return op->fcn(env,sexp->cdr);
		else if(cell_type(op) == VAL_LBA) {
			sexp = eval_lambda(env,cell_lba(op),sexp->cdr);
			return cell_lba(op)->ismacro ? eval(env,sexp) : sexp;
		}
	}

	error("unhandled s-expression of type %i",cell_type(sexp));

	return NULL;
}

void print(cell_t *sexp) {
	if(!sexp) {
		printf("nil");
		return;
	}

	switch(cell_type(sexp)) {
	case VAL_SYM: printf("%s",sexp->sym);                        break;
	case VAL_I64: printf("%lli",sexp->i64);                      break;
	case VAL_DBL: printf("%f",sexp->dbl);                        break;
	case VAL_CHR: printf("'%c'",sexp->chr);                      break;
	case VAL_STR: printf("\"%.*s\"",(int) sexp->i64,sexp->data); break;
	case VAL_FCN: printf("<%p>",sexp->dbl);                      break;
	case VAL_LBA: printf("<%s>",cell_lba(sexp)->ismacro
		? "macro" : "lambda");                               break;

	case VAL_NIL:
	default:
		assert(cell_is_list(sexp));

		putchar('(');
		do {
			if(cell_is_list(sexp)) {
				print(sexp->car);
				if(sexp->cdr)
					putchar(' ');
			} else {
				printf(". ");
				print(sexp);
				break;
			}
		} while(sexp = sexp->cdr);
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

