#ifndef REPL_H
#define REPL_H

#include <setjmp.h>
#include <stdio.h>

#define PRESERVE_eval          env, sexp, op
#define PRESERVE_bind_args     env, envout, template, args, ismacro, head, tail
#define PRESERVE_eval_lambda   lambenv, lambp, body
#define PRESERVE_append        env, args, head, tail
#define PRESERVE_atom
#define PRESERVE_car
#define PRESERVE_cdr
#define PRESERVE_cond          env, args, pair
#define PRESERVE_cons          env, args, sexp
#define PRESERVE_eq            env, args, a
#define PRESERVE_gensym
#define PRESERVE_lambda
#define PRESERVE_macro
#define PRESERVE_macroexpand   env, sexp
#define PRESERVE_macroexpand_1 env, sexp
#define PRESERVE_print         env, args
#define PRESERVE_quasiquote
#define PRESERVE_quasiquote_unquote \
                               env, sexp, head, tail, splicep
#define PRESERVE_quote
#define PRESERVE_assign        env, sym
#define PRESERVE_add           env, args, dbl, i64, type
#define PRESERVE_sub           env, args, dbl, i64, type

#define EVAL_VARS \
	bool,      v, (ismacro, splice), \
	bool,    vpv, (splicep), \
	cell_t,   pv, (sexp, retval, op, template, args, head, body, pair, a, \
		b, sym, x), \
	cell_t, pvpv, (tail), \
	env_t,    pv, (env, envout, lambenv), \
	lambda_t, pv, (lambp), \
	double,    v, (dbl), \
	int64_t,   v, (i64)

struct cell;
struct env;
struct lambda;

extern int lineno;
extern char *filename;
extern jmp_buf checkjmp;

void builtin_init(struct env *);

void run_file(struct env *, FILE *);

struct cell *eval(struct env *, struct cell *);

#endif

