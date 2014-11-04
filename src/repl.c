#include <assert.h>
#include <setjmp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <unistd.h>

#include "cell.h"
#include "check.h"
#include "env.h"
#include "grammar.h"
#include "repl.h"
#include "token.h"
#include "util.h"

#define STACK_MAX_SIZE 10000000
#define STACK_GROWTH   1.4

typedef struct {
	size_t size;
	char *bottom, *top;
} stack_t;

int lineno;
char *filename;
jmp_buf checkjmp;

static char *str_t;
static char *str_unquote;
static char *str_unquote_splicing;

static cell_t *sym_t;

void *ParseAlloc(void *(*)(size_t));
void ParseFree(void *, void (*)(void *));
void Parse(void *, int, token_value_t, cell_t **);
void ParseTrace(FILE *, char *);

void print(cell_t *);

void builtin_init(env_t *env) {
	struct {
		char *name;
		fcn_t fcn;
	} *fcn, fcns[] = {
		{"append",       FCN_APPEND},
		{"atom",         FCN_ATOM},
		{"car",          FCN_CAR},
		{"cdr",          FCN_CDR},
		{"cond",         FCN_COND},
		{"cons",         FCN_CONS},
		{"eq",           FCN_EQ},
		{"eval",         FCN_EVAL},
		{"gensym",       FCN_GENSYM},
		{"lambda",       FCN_LAMBDA},
		{"macro",        FCN_MACRO},
		{"macroexpand",  FCN_MACROEXPAND},
		{"macroexpand-1",FCN_MACROEXPAND_1},
		{"print",        FCN_PRINT},
		{"quasiquote",   FCN_QUASIQUOTE},
		{"quote",        FCN_QUOTE},
		{"=",            FCN_ASSIGN},
		{"+",            FCN_ADD},
		{"-",            FCN_SUB},
		{NULL,0}
	};

	// Cache important symbols
	str_t = cell_str_intern("t",1);
	str_unquote = cell_str_intern("unquote",7);
	str_unquote_splicing = cell_str_intern("unquote-splicing",16);

	// Canonical truth symbol
	sym_t = cell_cons_t(VAL_SYM,str_t);
	env_set(env,str_t,sym_t,true);

	// Built-in functions
	for(fcn = fcns; fcn->name; fcn++)
		env_set(env,cell_str_intern(fcn->name,strlen(fcn->name)),
			cell_cons_t(VAL_FCN,fcn->fcn),true);
}

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

		if(tok == TOK_LPAREN) level++;
		if(tok == TOK_RPAREN) level--;

		if(!level && (tok == TOK_RPAREN || tok == TOK_SYMBOL))
			Parse(p,0,tokval,cell);
	} while(*cell == &sentinel && tok > 0);

	return *cell != &sentinel;
}

static void bind_args(env_t *env, env_t *envout, cell_t *template,
	cell_t *args, bool macro) {
	cell_t *head, **tail;

}

void grow_stack(stack_t *stack, size_t nbytes) {
	char *newstack;
	size_t newsize;

	if(stack->top - stack->bottom + nbytes > stack->size) {
		newsize = STACK_GROWTH*stack->size;
		if(newsize < stack->size + nbytes)
			newsize = stack->size + nbytes;
		if(newsize > STACK_MAX_SIZE)
			newsize = STACK_MAX_SIZE;
		check(newsize >= stack->size + nbytes,"stack overflow");
fprintf(stderr,"resizing stack: %12i -> %12i\n",stack->size,newsize);
		newstack = realloc(stack->bottom,
			newsize*sizeof *stack->bottom);
		check(newstack,"cannot grow stack");

		stack->size = newsize;
		stack->top = newstack + (stack->top - stack->bottom);
		stack->bottom = newstack;
	}
}

#define EXPAND(...) __VA_ARGS__

#define NARGS(...) NARGS__( \
	NARGS_HAS_COMMA(__VA_ARGS__), \
	NARGS_HAS_COMMA(NARGS_COMMA __VA_ARGS__ ()), \
	NARGS_(__VA_ARGS__,10,9,8,7,6,5,4,3,2,1) \
)
#define NARGS_(_1,_2,_3,_4,_5,_6,_7,_8,_9,_10,n, ...) n
#define NARGS_HAS_COMMA(...) NARGS_(__VA_ARGS__,1,1,1,1,1,1,1,1,1,0)
#define NARGS_COMMA() ,
#define NARGS__(a, b, n) NARGS___(a,b,n)
#define NARGS___(a, b, n) NARGS___##a##b(n)
#define NARGS___01(n) 0
#define NARGS___00(n) 1
#define NARGS___11(n) n

#define VAR_ARG(base, ...) VAR_ARG_(base,NARGS(__VA_ARGS__),__VA_ARGS__)
#define VAR_ARG_(base, num, ...) VAR_ARG__(base,num,__VA_ARGS__)
#define VAR_ARG__(base, num, ...) base##num (__VA_ARGS__)

#define STACK_ALLOC(type) ( \
	grow_stack(&stack,sizeof(type)), \
	stack.top += sizeof(type), \
	STACK_TOP(type) \
)

#define STACK_FREE(type) do { stack.top -= sizeof(type); } while(0)

#define STACK_TOP(type) (*(type *) (stack.top - sizeof(type)))

#define PUSH(var) do { \
	grow_stack(&stack,sizeof (var)); \
	memcpy(stack.top,(void *) &(var),sizeof (var)); \
	stack.top += sizeof (var); \
} while(0)

#define POP(var) do { \
	assert(stack.top - stack.bottom >= sizeof (var)); \
	stack.top -= sizeof (var); \
	memcpy((void *) &(var),stack.top,sizeof (var)); \
} while(0)

#define SAVE0()
#define SAVE1(arg)      PUSH(arg)
#define SAVE2(arg, ...) PUSH(arg); SAVE1(__VA_ARGS__)
#define SAVE3(arg, ...) PUSH(arg); SAVE2(__VA_ARGS__)
#define SAVE4(arg, ...) PUSH(arg); SAVE3(__VA_ARGS__)
#define SAVE5(arg, ...) PUSH(arg); SAVE4(__VA_ARGS__)
#define SAVE6(arg, ...) PUSH(arg); SAVE5(__VA_ARGS__)
#define SAVE7(arg, ...) PUSH(arg); SAVE6(__VA_ARGS__)

#define LOAD0()
#define LOAD1(arg)      POP(arg)
#define LOAD2(arg, ...) LOAD1(__VA_ARGS__); POP(arg)
#define LOAD3(arg, ...) LOAD2(__VA_ARGS__); POP(arg)
#define LOAD4(arg, ...) LOAD3(__VA_ARGS__); POP(arg)
#define LOAD5(arg, ...) LOAD4(__VA_ARGS__); POP(arg)
#define LOAD6(arg, ...) LOAD5(__VA_ARGS__); POP(arg)
#define LOAD7(arg, ...) LOAD6(__VA_ARGS__); POP(arg)

#define SAVE(...) VAR_ARG(SAVE,__VA_ARGS__)
#define LOAD(...) VAR_ARG(LOAD,__VA_ARGS__)

#define SET2(name, val)       name = (val)
#define SET4(name, val, ...)  name = (val); SET2(__VA_ARGS__)
#define SET6(name, val, ...)  name = (val); SET4(__VA_ARGS__)
#define SET8(name, val, ...)  name = (val); SET6(__VA_ARGS__)
#define SET10(name, val, ...) name = (val); SET8(__VA_ARGS__)

#define SET(...) VAR_ARG(SET,__VA_ARGS__)

#define CALL(fcn, ...) do { \
	EXPAND(SAVE(PRESERVE)); \
\
	EXPAND(SET(__VA_ARGS__)); \
\
	STACK_ALLOC(jmp_buf); \
	if(!setjmp(STACK_TOP(jmp_buf))) \
		goto fcn; \
	STACK_FREE(jmp_buf); \
\
	EXPAND(LOAD(PRESERVE)); \
} while(0)

#define RETURN(_val) do { \
	retval = (_val); \
\
	if(stack.top == stack.bottom) \
		return retval; \
\
	longjmp(STACK_TOP(jmp_buf),1); \
} while(0)

#define EVAL(_env, _sexp) \
	CALL(eval,env,(_env),sexp,(_sexp))
#define BIND_ARGS(_env, _envout, _template, _args, _ismacro) \
	CALL(bind_args,env,(_env),envout,(_envout),template,(_template), \
		args,(_args),ismacro,(_ismacro))
#define EVAL_LAMBDA(_env, _lambp, _args) \
	CALL(eval_lambda,env,(_env),lambp,(_lambp),args,(_args))
#define APPEND(_env, _args) \
	CALL(append,env,(_env),args,(_args))
#define ATOM(_env, _args) \
	CALL(atom,env,(_env),args,(_args))
#define CAR(_env, _args) \
	CALL(car,env,(_env),args,(_args))
#define CDR(_env, _args) \
	CALL(cdr,env,(_env),args,(_args))
#define COND(_env, _args) \
	CALL(cond,env,(_env),args,(_args))
#define CONS(_env, _args) \
	CALL(cons,env,(_env),args,(_args))
#define EQ(_env, _args) \
	CALL(eq,env,(_env),args,(_args))
#define GENSYM(_env, _args) \
	CALL(gensym,env,(_env),args,(_args))
#define LAMBDA(_env, _args) \
	CALL(lambda,env,(_env),args,(_args))
#define MACRO(_env, _args) \
	CALL(macro,env,(_env),args,(_args))
#define MACROEXPAND(_env, _args) \
	CALL(macroexpand,env,(_env),args,(_args))
#define MACROEXPAND_1(_env, _args) \
	CALL(macroexpand_1,env,(_env),args,(_args))
#define PRINT(_env, _args) \
	CALL(print,env,(_env),args,(_args))
#define QUASIQUOTE(_env, _args) \
	CALL(quasiquote,env,(_env),args,(_args))
#define QUASIQUOTE_UNQUOTE(_env, _sexp, _splicep) \
	CALL(quasiquote_unquote,env,(_env),sexp,(_sexp),splicep,(_splicep))
#define QUOTE(_env, _args) \
	CALL(quote,env,(_env),args,(_args))
#define ASSIGN(_env, _args) \
	CALL(assign,env,(_env),args,(_args))
#define ADD(_env, _args) \
	CALL(add,env,(_env),args,(_args))
#define SUB(_env, _args) \
	CALL(sub,env,(_env),args,(_args))

cell_t *eval(env_t *volatile env, cell_t *volatile sexp) {
	static int gensym_counter = 0;

	static stack_t stack = {
		.size = 4000,
		.bottom = NULL
	};

	cell_t *volatile retval;
	cell_t *volatile op;
	env_t *volatile envout;
	cell_t *volatile template;
	cell_t *volatile args;
	bool volatile ismacro;
	cell_t *volatile head, *volatile *volatile tail;
	env_t *volatile lambenv;
	lambda_t *volatile lambp;
	cell_t *volatile body;
	cell_t *volatile pair;
	cell_t *volatile a, *volatile b;
	char *str;
	lambda_t lamb;
	bool volatile splice, *volatile splicep;
	cell_t *volatile sym;
	double volatile dbl, xdbl;
	int64_t volatile i64, xi64;
	cell_type_t type;
	cell_t *x;

	// Initialize the stack
	if(!stack.bottom)
		stack.bottom = malloc(stack.size*sizeof *stack.bottom);
	stack.top = stack.bottom;

eval:
#undef PRESERVE
#define PRESERVE env, sexp, op
	if(!sexp)
		RETURN(sexp);

	switch(cell_type(sexp)) {
	case VAL_I64:
	case VAL_DBL:
	case VAL_CHR:
	case VAL_STR:
	case VAL_FCN:
		RETURN(sexp);

	case VAL_SYM:
		RETURN(env_get(env,sexp->sym,(cell_t **) &sexp) ? sexp : NULL);

	case VAL_NIL:
	default:
		assert(cell_is_list(sexp));

		EVAL(env,sexp->car);
		op = retval;
		check(op && (cell_type(op) == VAL_FCN
			|| cell_type(op) == VAL_LBA),
			"operator must be a function");
		if(cell_type(op) == VAL_FCN) {
			sexp = sexp->cdr;
			switch(op->fcn) {
			case FCN_APPEND:        APPEND(env,sexp);        break;
			case FCN_ATOM:          ATOM(env,sexp);          break;
			case FCN_CAR:           CAR(env,sexp);           break;
			case FCN_CDR:           CDR(env,sexp);           break;
			case FCN_COND:          COND(env,sexp);          break;
			case FCN_CONS:          CONS(env,sexp);          break;
			case FCN_EQ:            EQ(env,sexp);            break;
			case FCN_EVAL:          EVAL(env,sexp);          break;
			case FCN_GENSYM:        GENSYM(env,sexp);        break;
			case FCN_LAMBDA:        LAMBDA(env,sexp);        break;
			case FCN_MACRO:         MACRO(env,sexp);         break;
			case FCN_MACROEXPAND:   MACROEXPAND(env,sexp);   break;
			case FCN_MACROEXPAND_1: MACROEXPAND_1(env,sexp); break;
			case FCN_PRINT:         PRINT(env,sexp);         break;
			case FCN_QUASIQUOTE:    QUASIQUOTE(env,sexp);    break;
			case FCN_QUOTE:         QUOTE(env,sexp);         break;
			case FCN_ASSIGN:        ASSIGN(env,sexp);        break;
			case FCN_ADD:           ADD(env,sexp);           break;
			case FCN_SUB:           SUB(env,sexp);           break;
			}
			RETURN(retval);
		} else if(cell_type(op) == VAL_LBA) {
			EVAL_LAMBDA(env,cell_lba(op),sexp->cdr);
			if(cell_lba(op)->ismacro)
				EVAL(env,retval);
			RETURN(retval);
		}
	}

	error("unhandled s-expression of type %i",cell_type(sexp));

	RETURN(NULL);

bind_args:
#undef PRESERVE
#define PRESERVE env, envout, template, args, ismacro, head, tail
	for(; args && template; args = args->cdr, template = template->cdr) {
		// Skip nils in the template
		while(!template->car)
			if(template = template->cdr, !template)
				break;

		if(cell_type(template) == VAL_SYM) { // Var-args
			// Evaluate the args for normal lambdas
			for(head = NULL, tail = &head; !ismacro && args;
				args = args->cdr, tail = &(*tail)->cdr) {
				EVAL(env,args->car);
				*tail = cell_cons(retval,NULL);
			}

			env_set(envout,template->sym,ismacro ? args : head,
				true);
			break;
		} else if(cell_is_list(template->car)
			&& cell_is_list(args->car))
			BIND_ARGS(env,envout,template->car,args->car,ismacro);
		else if(cell_type(template->car) == VAL_SYM) {
			if(ismacro)
				retval = args->car;
			else EVAL(env,args->car);

			env_set(envout,template->car->sym,retval,true);
		} else check(false,"mal-formed macro arguments");
	}

	check(cell_is_atom(template) || !template,
		"too few arguments to macro expression");

	RETURN(NULL);

eval_lambda:
#undef PRESERVE
#define PRESERVE lambenv, lambp, body
	lambenv = env_cons(lambp->env);

	// Bind the arguments
	BIND_ARGS(env,lambenv,lambp->args,args,lambp->ismacro);

	// Evaluate the body
	for(body = lambp->body; body; body = body->cdr)
		EVAL(lambenv,body->car);

	env_free(lambenv);

	RETURN(retval);

append:
#undef PRESERVE
#define PRESERVE env, args, head, tail
	for(head = NULL, tail = &head; args; args = args->cdr) {
		EVAL(env,args->car);

		check(cell_is_list(retval),
			"arguments to append must be lists");

		if(args->cdr) {
			for(; retval; retval = retval->cdr) {
				*tail = cell_dup(retval);
				tail = &(*tail)->cdr;
			}
		} else *tail = retval;
	}

	RETURN(head);

atom:
#undef PRESERVE
#define PRESERVE
	check(args && !args->cdr,"too many arguments to atom");

	EVAL(env,args->car);
	RETURN(cell_is_atom(retval) ? sym_t : NULL);

car:
#undef PRESERVE
#define PRESERVE
	check(args && !args->cdr,"too many arguments to car");
	EVAL(env,args->car);

	check(retval && cell_is_list(retval),
		"argument to car must be a non-empty list");

	RETURN(retval->car);

cdr:
#undef PRESERVE
#define PRESERVE
	check(args && !args->cdr,"too many arguments to cdr");
	EVAL(env,args->car);

	check(retval && cell_is_list(retval),
		"argument to cdr must be a non-empty list");

	RETURN(retval->cdr);

cond:
#undef PRESERVE
#define PRESERVE env, args, pair
	for(; args; args = args->cdr) {
		pair = args->car;
		check(pair && pair->cdr && !pair->cdr->cdr,
			"argument to cond must be a pair");

		EVAL(env,pair->car);
		if(retval) {
			EVAL(env,pair->cdr->car);
			RETURN(retval);
		}
	}

	RETURN(NULL);

cons:
#undef PRESERVE
#define PRESERVE env, args, sexp
	check(args && args->cdr,"too few arguments to cons");
	check(!args->cdr->cdr,"too many arguments to cons");

	EVAL(env,args->car);
	sexp = retval;
	EVAL(env,args->cdr->car);

	RETURN(cell_cons(sexp,retval));

eq:
#undef PRESERVE
#define PRESERVE env, args, a
	check(args && args->cdr,"too few arguments to eq");
	check(!args->cdr->cdr,"too many arguments to eq");

	EVAL(env,args->car);
	a = retval;
	EVAL(env,args->cdr->car);
	b = retval;

	if(!a || !b)
		RETURN(a || b ? NULL : sym_t);

	if(a == b)
		RETURN(sym_t);

	if(cell_type(a) != cell_type(b))
		RETURN(NULL);

	switch(cell_type(a)) {
	case VAL_SYM: RETURN(a->sym == b->sym ? sym_t : NULL);
	case VAL_I64: RETURN(a->i64 == b->i64 ? sym_t : NULL);
	case VAL_DBL: RETURN(a->dbl == b->dbl ? sym_t : NULL);
	case VAL_CHR: RETURN(a->chr == b->chr ? sym_t : NULL);
	case VAL_FCN: RETURN(a->fcn == b->fcn ? sym_t : NULL);

	case VAL_STR:
		RETURN(a->i64 == b->i64 && memcmp(a->data,b->data,a->i64) == 0
			? sym_t : NULL);

	case VAL_LBA:
		RETURN(memcmp(cell_lba(a),cell_lba(b),sizeof(lambda_t)) == 0
			? sym_t : NULL);

	case VAL_LST:
		RETURN(NULL);

	default:
		error("unhandled value in eq, type %i",cell_type(a));
		RETURN(NULL);
	}

gensym:
#undef PRESERVE
#define PRESERVE
	check(!args,"too many arguments to gensym");

	str = malloc(1 + 5 + 1); // TODO: GC this
	sprintf(str,"G%05i",gensym_counter++%100000);

	RETURN(cell_cons_t(VAL_SYM,str));

lambda:
#undef PRESERVE
#define PRESERVE
	check(args,"missing lambda parameter list");

	// Set up the lambda
	lamb.ismacro = false;
	lamb.env = env_ref(env);
	lamb.args = args->car;
	lamb.body = args->cdr;

	RETURN(cell_cons_t(VAL_LBA,&lamb));

macro:
#undef PRESERVE
#define PRESERVE
	check(args,"missing macro parameter list");

	// Set up the macro
	lamb.ismacro = true;
	lamb.env = env_ref(env);
	lamb.args = args->car;
	lamb.body = args->cdr;

	RETURN(cell_cons_t(VAL_LBA,&lamb));

macroexpand:
#undef PRESERVE
#define PRESERVE env, sexp
	check(args && !args->cdr,
		"incorrect number of arguments to macroexpand");

	EVAL(env,args->car);
	sexp = retval;

	while(true) {
		if(cell_is_atom(sexp))
			RETURN(sexp);

		EVAL(env,sexp->car);
		op = retval;
		if(!op || cell_type(op) != VAL_LBA || !cell_lba(op)->ismacro)
			RETURN(sexp);

		EVAL_LAMBDA(NULL,cell_lba(op),sexp->cdr);
		sexp = retval;
	}

	RETURN(sexp);

macroexpand_1:
#undef PRESERVE
#define PRESERVE env, sexp
	check(args && !args->cdr,
		"incorrect number of arguments to macroexpand-1");

	EVAL(env,args->car);
	sexp = retval;

	if(cell_is_atom(sexp))
		RETURN(sexp);

	EVAL(env,sexp->car);
	op = retval;
	if(!op || cell_type(op) != VAL_LBA || !cell_lba(op)->ismacro)
		RETURN(sexp);

	EVAL_LAMBDA(NULL,cell_lba(op),sexp->cdr);
	RETURN(retval);

print:
#undef PRESERVE
#define PRESERVE env, args
	for(; args; args = args->cdr) {
		EVAL(env,args->car);
		print(retval);
	}

	RETURN(NULL);

quasiquote:
#undef PRESERVE
#define PRESERVE
	check(args,"too few arguments to quasiquote");
	check(!args->cdr,"too many arguments to quasiquote");

	sexp = args->car;
	splicep = NULL;

quasiquote_unquote:
#undef PRESERVE
#define PRESERVE env, sexp, head, tail, splicep
	// Default
	if(splicep)
		*splicep = false;

	// atom
	if(cell_is_atom(sexp))
		RETURN(sexp);

	// ,sexp
	if(cell_type(sexp->car) == VAL_SYM
		&& sexp->car->sym == str_unquote) {
		sexp = sexp->cdr;

		check(sexp,"too few arguments to unquote");
		check(!sexp->cdr,"too many arguments to unquote");

		EVAL(env,sexp->car);
		RETURN(retval);
	}

	// ,@sexp
	if(cell_type(sexp->car) == VAL_SYM
		&& sexp->car->sym == str_unquote_splicing) {
		sexp = sexp->cdr;

		check(splicep,"syntax `,@sexp is undefined");
		check(sexp,"too few arguments to unquote-splicing");
		check(!sexp->cdr,"too many arguments to unquote-splicing");

		*splicep = true;

		EVAL(env,sexp->car);
		RETURN(retval);
	}

	// (... ,sexp ... ,@sexp ...)
	for(head = sexp, tail = &head; sexp; sexp = sexp->cdr) {
		QUASIQUOTE_UNQUOTE(env,sexp->car,&splice);

		if(splice)
			*tail = retval;
		else *tail = cell_cons(retval,NULL);

		for(; *tail; tail = &(*tail)->cdr);
	}

	// Default again
	if(splicep)
		*splicep = false;

	RETURN(head);

quote:
#undef PRESERVE
#define PRESERVE
	check(args,"too few arguments to quote");
	check(!args->cdr,"too many arguments to quote");

	RETURN(args->car);

assign:
#undef PRESERVE
#define PRESERVE env, sym
	check(args && args->cdr,"too few arguments to =");
	check(!args->cdr->cdr,"too many arguments to =");

	// The symbol to assign to
	sym = args->car;
	check(sym && cell_type(sym) == VAL_SYM,
		"first argument to = not a symbol");

	// The value to assign
	EVAL(env,args->cdr->car);

	// Assign it!
	env_set(env,sym->sym,retval,false);

	RETURN(retval);

add:
#undef PRESERVE
#define PRESERVE env, args, dbl, i64, type
	dbl = i64 = 0;
	type = VAL_NIL;
	for(; args; args = args->cdr) {
		EVAL(env,args->car);
		x = retval;

		check(x && (cell_type(x) == VAL_I64
			|| cell_type(x) == VAL_DBL),
			"argument to + not a number");

		switch(cell_type(x)) {
		case VAL_I64: xdbl = xi64 = x->i64; break;
		case VAL_DBL: xi64 = xdbl = x->dbl; break;
		}

add_x:
		switch(type) {
		case VAL_NIL: type = cell_type(x); goto add_x;

		case VAL_I64: i64 += xi64; break;
		case VAL_DBL: dbl += xdbl; break;
		}
	}

	switch(type) {
	case VAL_NIL: RETURN(cell_cons_t(VAL_DBL,0.));
	case VAL_I64: RETURN(cell_cons_t(VAL_I64,i64));
	case VAL_DBL: RETURN(cell_cons_t(VAL_DBL,dbl));
	}

sub:
#undef PRESERVE
#define PRESERVE env, args, dbl, i64, type
	dbl = i64 = 0;
	type = VAL_NIL;
	for(; args; args = args->cdr) {
		EVAL(env,args->car);
		x = retval;

		check(x && (cell_type(x) == VAL_I64
			|| cell_type(x) == VAL_DBL),
			"argument to - not a number");

		switch(cell_type(x)) {
		case VAL_I64: xdbl = xi64 = x->i64; break;
		case VAL_DBL: xi64 = xdbl = x->dbl; break;
		}

sub_x:
		switch(type) {
		case VAL_NIL:
			type = cell_type(x);

			if(args->cdr) {
				xdbl = -xdbl;
				xi64 = -xi64;
			}

			goto sub_x;

		case VAL_I64: i64 -= xi64; break;
		case VAL_DBL: dbl -= xdbl; break;
		}
	}

	switch(type) {
	case VAL_NIL: RETURN(cell_cons_t(VAL_DBL,0.));
	case VAL_I64: RETURN(cell_cons_t(VAL_I64,i64));
	case VAL_DBL: RETURN(cell_cons_t(VAL_DBL,dbl));
	}
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
	case VAL_FCN: printf("<fcn>");                               break;
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

