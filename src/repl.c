#include <assert.h>
#include <inttypes.h>
#include <setjmp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <unistd.h>

#include "cell.h"
#include "check.h"
#include "env.h"
#include "grammar.h"
#include "mem.h"
#include "repl.h"
#include "stack.h"
#include "token.h"
#include "util.h"
#include "va_macro.h"

#define STACK_MAX_SIZE 10000000
#define STACK_GROWTH   1.4

char *filename;
jmp_buf checkjmp;
stream_t *currentstream;

static string_t *str_t;
static string_t *str_unquote;
static string_t *str_unquote_splicing;

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
	str_t = INTERN_CONST_STRING("t");
	str_unquote = INTERN_CONST_STRING("unquote");
	str_unquote_splicing = INTERN_CONST_STRING("unquote-splicing");

	// Canonical truth symbol
	sym_t = cell_cons_t(VAL_SYM,str_t);
	env_set(env,str_t,sym_t,true);

	// Built-in functions
	for(fcn = fcns; fcn->name; fcn++)
		env_set(env,INTERN_CONST_STRING(fcn->name),
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

void grow_stack(stack_t *stack, size_t nbytes) {
	char *newstack;
	size_t newsize;

	newsize = STACK_GROWTH*stack->size;
	if(newsize < stack->size + nbytes)
		newsize = stack->size + nbytes;
	if(newsize > STACK_MAX_SIZE)
		newsize = STACK_MAX_SIZE;
	check(newsize >= stack->size + nbytes,"stack overflow");

	debug("resizing stack: %12li -> %12li",(long) stack->size,
		(long) newsize);

	newstack = realloc(stack->bottom,
		newsize*sizeof *stack->bottom);
	check(newstack,"cannot grow stack");

	stack->size = newsize;
	stack->top = newstack + (stack->top - stack->bottom);
	stack->bottom = newstack;
}

#define QUAL_v     volatile
#define QUAL_pv   *volatile
#define QUAL_vpv   volatile *volatile
#define QUAL_pvpv *volatile *volatile

#define SAVE0()
#define SAVE1(arg)      STACK_PUSH(stack,arg)
#define SAVE2(arg, ...) STACK_PUSH(stack,arg); SAVE1(__VA_ARGS__)
#define SAVE3(arg, ...) STACK_PUSH(stack,arg); SAVE2(__VA_ARGS__)
#define SAVE4(arg, ...) STACK_PUSH(stack,arg); SAVE3(__VA_ARGS__)
#define SAVE5(arg, ...) STACK_PUSH(stack,arg); SAVE4(__VA_ARGS__)
#define SAVE6(arg, ...) STACK_PUSH(stack,arg); SAVE5(__VA_ARGS__)
#define SAVE7(arg, ...) STACK_PUSH(stack,arg); SAVE6(__VA_ARGS__)

#define LOAD0()
#define LOAD1(arg)      STACK_POP(stack,arg)
#define LOAD2(arg, ...) LOAD1(__VA_ARGS__); STACK_POP(stack,arg)
#define LOAD3(arg, ...) LOAD2(__VA_ARGS__); STACK_POP(stack,arg)
#define LOAD4(arg, ...) LOAD3(__VA_ARGS__); STACK_POP(stack,arg)
#define LOAD5(arg, ...) LOAD4(__VA_ARGS__); STACK_POP(stack,arg)
#define LOAD6(arg, ...) LOAD5(__VA_ARGS__); STACK_POP(stack,arg)
#define LOAD7(arg, ...) LOAD6(__VA_ARGS__); STACK_POP(stack,arg)

#define SAVE(func) SAVE_(func)
#define SAVE_(func) SAVE__(PRESERVE_##func)
#define SAVE__(...) VAR_ARG(SAVE,__VA_ARGS__)

#define LOAD(func) LOAD_(func)
#define LOAD_(func) LOAD__(PRESERVE_##func)
#define LOAD__(...) VAR_ARG(LOAD,__VA_ARGS__)

#define SET2(name, val)       name = (val)
#define SET4(name, val, ...)  name = (val); SET2(__VA_ARGS__)
#define SET6(name, val, ...)  name = (val); SET4(__VA_ARGS__)
#define SET8(name, val, ...)  name = (val); SET6(__VA_ARGS__)
#define SET10(name, val, ...) name = (val); SET8(__VA_ARGS__)

#define SET(...) VAR_ARG(SET,__VA_ARGS__)

#define LABEL FUNCTION:

#define CALL(fcn, ...) do { \
	*STACK_ALLOC(stack,enum builtin) = PREFIX_BUILTIN(,FUNCTION); \
	SAVE(FUNCTION); \
\
	SET(__VA_ARGS__); \
\
	(void) STACK_ALLOC(stack,jmp_buf); \
	if(!setjmp(*STACK_TOP(stack,jmp_buf))) \
		goto fcn; \
	STACK_FREE(stack,jmp_buf); \
\
	mem_gc(&stack); \
\
	LOAD(FUNCTION); \
	STACK_FREE(stack,enum builtin); \
} while(0)

#define JMP(fcn, ...) do { \
	SET(__VA_ARGS__); \
	goto fcn; \
} while(0)

#define RETURN(_val) do { \
	retval = (_val); \
\
	if(stack.top == stack.bottom) \
		goto done; \
\
	longjmp(*STACK_TOP(stack,jmp_buf),1); \
} while(0)

#define EVAL(_env, _sexp) \
	CALL(eval,env,(_env),sexp,(_sexp))
#define JMP_EVAL(_env, _sexp) \
	JMP(eval,env,(_env),sexp,(_sexp))
#define BIND_ARGS(_env, _envout, _template, _args, _ismacro) \
	CALL(bind_args,env,(_env),envout,(_envout),template,(_template), \
		args,(_args),ismacro,(_ismacro))
#define EVAL_LAMBDA(_env, _lambp, _args) \
	CALL(eval_lambda,env,(_env),lambp,(_lambp),args,(_args))
#define JMP_EVAL_LAMBDA(_env, _lambp, _args) \
	JMP(eval_lambda,env,(_env),lambp,(_lambp),args,(_args))
#define JMP_APPEND(_env, _args) \
	JMP(append,env,(_env),args,(_args))
#define JMP_ATOM(_env, _args) \
	JMP(atom,env,(_env),args,(_args))
#define JMP_CAR(_env, _args) \
	JMP(car,env,(_env),args,(_args))
#define JMP_CDR(_env, _args) \
	JMP(cdr,env,(_env),args,(_args))
#define JMP_COND(_env, _args) \
	JMP(cond,env,(_env),args,(_args))
#define JMP_CONS(_env, _args) \
	JMP(cons,env,(_env),args,(_args))
#define JMP_EQ(_env, _args) \
	JMP(eq,env,(_env),args,(_args))
#define JMP_GENSYM(_env, _args) \
	JMP(gensym,env,(_env),args,(_args))
#define JMP_LAMBDA(_env, _args) \
	JMP(lambda,env,(_env),args,(_args))
#define JMP_MACRO(_env, _args) \
	JMP(macro,env,(_env),args,(_args))
#define JMP_MACROEXPAND(_env, _args) \
	JMP(macroexpand,env,(_env),args,(_args))
#define JMP_MACROEXPAND_1(_env, _args) \
	JMP(macroexpand_1,env,(_env),args,(_args))
#define JMP_PRINT(_env, _args) \
	JMP(print,env,(_env),args,(_args))
#define JMP_QUASIQUOTE(_env, _args) \
	JMP(quasiquote,env,(_env),args,(_args))
#define QUASIQUOTE_UNQUOTE(_env, _sexp, _splicep) \
	CALL(quasiquote_unquote,env,(_env),sexp,(_sexp),splicep,(_splicep))
#define JMP_QUOTE(_env, _args) \
	JMP(quote,env,(_env),args,(_args))
#define JMP_ASSIGN(_env, _args) \
	JMP(assign,env,(_env),args,(_args))
#define JMP_ADD(_env, _args) \
	JMP(add,env,(_env),args,(_args))
#define JMP_SUB(_env, _args) \
	JMP(sub,env,(_env),args,(_args))

cell_t *eval(env_t *_env, cell_t *_sexp) {
	static int gensym_counter = 0;

	static stack_t stack = {
		.size = 4000,
		.bottom = NULL
	};

	static uint32_t retvalh = ~(uint32_t) 0;

	EXPAND(EACH(PRINT_VARS,(;),(),EVAL_VARS));

	char *str;
	lambda_t lamb;
	double xdbl;
	int64_t xi64;

	// Initialize the variables
	env = _env;
	sexp = _sexp;

	// Initialize the stack
	if(!stack.bottom)
		stack.bottom = malloc(stack.size*sizeof *stack.bottom);
	stack.top = stack.bottom;

	// Register retval with the garbage collector
	if(retvalh == ~(uint32_t) 0)
		retvalh = mem_new_handle(GC_TYPE_INDIRECT(cell_t));
	mem_set_handle(retvalh,(void *) &retval);

#undef FUNCTION
#define FUNCTION eval
LABEL
	op = NULL;

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
			case FCN_APPEND:        JMP_APPEND(env,sexp);
			case FCN_ATOM:          JMP_ATOM(env,sexp);
			case FCN_CAR:           JMP_CAR(env,sexp);
			case FCN_CDR:           JMP_CDR(env,sexp);
			case FCN_COND:          JMP_COND(env,sexp);
			case FCN_CONS:          JMP_CONS(env,sexp);
			case FCN_EQ:            JMP_EQ(env,sexp);
			case FCN_GENSYM:        JMP_GENSYM(env,sexp);
			case FCN_LAMBDA:        JMP_LAMBDA(env,sexp);
			case FCN_MACRO:         JMP_MACRO(env,sexp);
			case FCN_MACROEXPAND:   JMP_MACROEXPAND(env,sexp);
			case FCN_MACROEXPAND_1: JMP_MACROEXPAND_1(env,sexp);
			case FCN_PRINT:         JMP_PRINT(env,sexp);
			case FCN_QUASIQUOTE:    JMP_QUASIQUOTE(env,sexp);
			case FCN_QUOTE:         JMP_QUOTE(env,sexp);
			case FCN_ASSIGN:        JMP_ASSIGN(env,sexp);
			case FCN_ADD:           JMP_ADD(env,sexp);
			case FCN_SUB:           JMP_SUB(env,sexp);

			case FCN_EVAL:
				check(sexp,"too few arguments to eval");
				check(!sexp->cdr,"too many arguments to eval");

				EVAL(env,sexp->car);
				JMP_EVAL(env,retval);
			}

			check(false,"unhandled function type");
		} else if(cell_type(op) == VAL_LBA) {
			if(cell_lba(op)->ismacro) {
				EVAL_LAMBDA(env,cell_lba(op),sexp->cdr);
				JMP_EVAL(env,retval);
			} else JMP_EVAL_LAMBDA(env,cell_lba(op),sexp->cdr);
		}
	}

	error("unhandled s-expression of type %i",cell_type(sexp));

	RETURN(NULL);

#undef FUNCTION
#define FUNCTION bind_args
LABEL
	head = NULL;
	tail = NULL;

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

#undef FUNCTION
#define FUNCTION eval_lambda
LABEL
	lambenv = env_cons(lambp->env);

	// Bind the arguments
	BIND_ARGS(env,lambenv,lambp->args,args,lambp->ismacro);

	// Evaluate the body
	for(body = lambp->body; body && body->cdr; body = body->cdr)
		EVAL(lambenv,body->car);

	// Jump right to the last body expression
	JMP_EVAL(lambenv,body->car);

#undef FUNCTION
#define FUNCTION append
LABEL
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

#undef FUNCTION
#define FUNCTION atom
LABEL
	check(args && !args->cdr,"too many arguments to atom");

	EVAL(env,args->car);
	RETURN(cell_is_atom(retval) ? sym_t : NULL);

#undef FUNCTION
#define FUNCTION car
LABEL
	check(args && !args->cdr,"too many arguments to car");
	EVAL(env,args->car);

	check(retval && cell_is_list(retval),
		"argument to car must be a non-empty list");

	RETURN(retval->car);

#undef FUNCTION
#define FUNCTION cdr
LABEL
	check(args && !args->cdr,"too many arguments to cdr");
	EVAL(env,args->car);

	check(retval && cell_is_list(retval),
		"argument to cdr must be a non-empty list");

	RETURN(retval->cdr);

#undef FUNCTION
#define FUNCTION cond
LABEL
	for(; args; args = args->cdr) {
		pair = args->car;
		check(pair && pair->cdr && !pair->cdr->cdr,
			"argument to cond must be a pair");

		EVAL(env,pair->car);
		if(retval)
			JMP_EVAL(env,pair->cdr->car);
	}

	RETURN(NULL);

#undef FUNCTION
#define FUNCTION cons
LABEL
	sexp = NULL;

	check(args && args->cdr,"too few arguments to cons");
	check(!args->cdr->cdr,"too many arguments to cons");

	EVAL(env,args->car);
	sexp = retval;
	EVAL(env,args->cdr->car);

	RETURN(cell_cons(sexp,retval));

#undef FUNCTION
#define FUNCTION eq
LABEL
	a = NULL;

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
	case VAL_STR: RETURN(NULL);
	case VAL_LBA: RETURN(NULL);
	case VAL_LST: RETURN(NULL);

	default:
		error("unhandled value in eq, type %i",cell_type(a));
		RETURN(NULL);
	}

#undef FUNCTION
#define FUNCTION gensym
LABEL
	check(!args,"too many arguments to gensym");

	str = mem_alloc(1 + 5 + 1);
	sprintf(str,"G%05i",gensym_counter++%100000);

	RETURN(cell_cons_t(VAL_SYM,str));

#undef FUNCTION
#define FUNCTION lambda
LABEL
	check(args,"missing lambda parameter list");

	// Set up the lambda
	lamb.ismacro = false;
	lamb.env = env;
	lamb.args = args->car;
	lamb.body = args->cdr;

	RETURN(cell_cons_t(VAL_LBA,&lamb));

#undef FUNCTION
#define FUNCTION macro
LABEL
	check(args,"missing macro parameter list");

	// Set up the macro
	lamb.ismacro = true;
	lamb.env = env;
	lamb.args = args->car;
	lamb.body = args->cdr;

	RETURN(cell_cons_t(VAL_LBA,&lamb));

#undef FUNCTION
#define FUNCTION macroexpand
LABEL
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

#undef FUNCTION
#define FUNCTION macroexpand_1
LABEL
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

	JMP_EVAL_LAMBDA(NULL,cell_lba(op),sexp->cdr);

#undef FUNCTION
#define FUNCTION print
LABEL
	for(; args; args = args->cdr) {
		EVAL(env,args->car);
		print(retval);
	}

	RETURN(NULL);

#undef FUNCTION
#define FUNCTION quasiquote
LABEL
	check(args,"too few arguments to quasiquote");
	check(!args->cdr,"too many arguments to quasiquote");

	sexp = args->car;
	splicep = NULL;

#undef FUNCTION
#define FUNCTION quasiquote_unquote
LABEL
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

		JMP_EVAL(env,sexp->car);
	}

	// ,@sexp
	if(cell_type(sexp->car) == VAL_SYM
		&& sexp->car->sym == str_unquote_splicing) {
		sexp = sexp->cdr;

		check(splicep,"syntax `,@sexp is undefined");
		check(sexp,"too few arguments to unquote-splicing");
		check(!sexp->cdr,"too many arguments to unquote-splicing");

		*splicep = true;

		JMP_EVAL(env,sexp->car);
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

#undef FUNCTION
#define FUNCTION quote
LABEL
	check(args,"too few arguments to quote");
	check(!args->cdr,"too many arguments to quote");

	RETURN(args->car);

#undef FUNCTION
#define FUNCTION assign
LABEL
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

#undef FUNCTION
#define FUNCTION add
LABEL
	dbl = i64 = 0;
	type = VAL_NIL;
	for(; args; args = args->cdr) {
		EVAL(env,args->car);
		x = retval;

		switch(cell_type(x)) {
		case VAL_I64: xdbl = xi64 = x->i64; break;
		case VAL_DBL: xi64 = xdbl = x->dbl; break;

		default: check(false,"argument to + not a number");
		}

add_x:
		switch(type) {
		case VAL_NIL: type = cell_type(x); goto add_x;

		case VAL_I64: i64 += xi64; break;
		case VAL_DBL: dbl += xdbl; break;

		default: break;
		}
	}

	switch(type) {
	case VAL_NIL: RETURN(cell_cons_t(VAL_DBL,0.));
	case VAL_I64: RETURN(cell_cons_t(VAL_I64,i64));
	case VAL_DBL: RETURN(cell_cons_t(VAL_DBL,dbl));

	default: check(false,"impossible");;
	}

#undef FUNCTION
#define FUNCTION sub
LABEL
	dbl = i64 = 0;
	type = VAL_NIL;
	for(; args; args = args->cdr) {
		EVAL(env,args->car);
		x = retval;

		switch(cell_type(x)) {
		case VAL_I64: xdbl = xi64 = x->i64; break;
		case VAL_DBL: xi64 = xdbl = x->dbl; break;

		default: check(false,"argument to - not a number");
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

		default: break;
		}
	}

	switch(type) {
	case VAL_NIL: RETURN(cell_cons_t(VAL_DBL,0.));
	case VAL_I64: RETURN(cell_cons_t(VAL_I64,i64));
	case VAL_DBL: RETURN(cell_cons_t(VAL_DBL,dbl));

	default: check(false,"impossible");
	}

// Cleanup when actually returning
done:
	mem_set_handle(retvalh,NULL);

	return retval;
}

void print(cell_t *sexp) {
	if(!sexp) {
		printf("nil");
		return;
	}

	switch(cell_type(sexp)) {
	case VAL_SYM: printf("%.*s",(int) sexp->sym->len,sexp->sym->str);
		break;
	case VAL_I64: printf("%" PRId64,sexp->i64);                  break;
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
	cell_t *sexp;

	// Set up the parser
	p = ParseAlloc(malloc);
	currentstream = stream_cons_f(in);

	// Catch check failures (i.e., run-time errors)
	setjmp(checkjmp);

	while(true) {
		if(!readf(p,currentstream,&sexp))
			break;

		sexp = eval(env,sexp);

		if(stream_interactive(currentstream)) {
			print(sexp);
			putchar('\n');
		}
	}

	if(stream_interactive(currentstream))
		putchar('\n');

	// Clean up
	stream_free(currentstream);
	currentstream = NULL;
	ParseFree(p,free);
}

