#ifndef REPL_H
#define REPL_H

#include <setjmp.h>
#include <stdio.h>

#include "va_macro.h"

#define BUILTINS eval, bind_args, eval_lambda, append, atom, car, cdr, cond, \
	cons, eq, gensym, lambda, macro, macroexpand, macroexpand_1, print, \
	quasiquote, quasiquote_unquote, quote, assign, add, sub

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
                               env, sexp, toplevel, head, tail
#define PRESERVE_quote
#define PRESERVE_assign        env, sym
#define PRESERVE_add           env, args, dbl, i64, type
#define PRESERVE_sub           env, args, dbl, i64, type

#define EVAL_VARS \
	(bool,       v, (ismacro, splice, toplevel)), \
	(cell_t,    pv, (sexp, retval, op, template, args, head, body, pair, \
		a, b, sym, x)), \
	(cell_t,  pvpv, (tail)), \
	(cell_type_t,v, (type)), \
	(env_t,     pv, (env, envout, lambenv)), \
	(lambda_t,  pv, (lambp)), \
	(double,     v, (dbl)), \
	(int64_t,    v, (i64))

#define PREFIX_BUILTIN(all, x) PREFIX_BUILTIN_(x)
#define PREFIX_BUILTIN_(x) BUILTIN_##x

#define PRINT_VAR(type, var) type var

#define PRINT_VARS(all, def) PRINT_VARS_ def
#define PRINT_VARS_(type, qual, vars) \
	DEFER(EACH_INDIRECT)()(PRINT_VAR,(;),(type QUAL_##qual),LITERAL vars)

enum builtin {
	EACH(PREFIX_BUILTIN,(,),(),BUILTINS)
};

struct env;

extern char *filename;
extern jmp_buf checkjmp;
extern struct stream *currentstream;

void builtin_init(struct env *);

void run_file(struct env *, FILE *);

#endif

