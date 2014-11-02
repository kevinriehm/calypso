#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "cell.h"
#include "check.h"
#include "env.h"
#include "repl.h"
#include "util.h"

static char *str_t;
static char *str_unquote;
static char *str_unquote_splicing;

static cell_t *sym_t;

static cell_t *append(env_t *env, cell_t *args) {
	cell_t *head, **tail, *val;

	for(head = NULL, tail = &head; args; args = args->cdr) {
		val = eval(env,args->car);

		check(cell_is_list(val),"arguments to append must be lists");

		if(args->cdr) {
			for(; val; val = val->cdr) {
				*tail = cell_dup(val);
				tail = &(*tail)->cdr;
			}
		} else *tail = val;
	}

	return head;
}

static cell_t *atom(env_t *env, cell_t *args) {
	check(args && !args->cdr,"too many arguments to atom");

	return cell_is_atom(eval(env,args->car)) ? sym_t : NULL;
}

static cell_t *car(env_t *env, cell_t *args) {
	check(args && !args->cdr,"too many arguments to car");
	args = eval(env,args->car);

	check(args && cell_is_list(args),
		"argument to car must be a non-empty list");

	return args->car;
}

static cell_t *cdr(env_t *env, cell_t *args) {
	check(args && !args->cdr,"too many arguments to cdr");
	args = eval(env,args->car);

	check(args && cell_is_list(args),
		"argument to cdr must be a non-empty list");

	return args->cdr;
}

static cell_t *cond(env_t *env, cell_t *args) {
	cell_t *pair;

	for(; args; args = args->cdr) {
		pair = args->car;
		check(pair && pair->cdr && !pair->cdr->cdr,
			"argument to cond must be a pair");

		if(eval(env,pair->car))
			return eval(env,pair->cdr->car);
	}

	return NULL;
}

static cell_t *cons(env_t *env, cell_t *args) {
	cell_t *a, *b;

	check(args && args->cdr,"too few arguments to cons");
	check(!args->cdr->cdr,"too many arguments to cons");

	a = eval(env,args->car);
	args = args->cdr;
	b = eval(env,args->car);

	return cell_cons(a,b);
}

static cell_t *eq(env_t *env, cell_t *args) {
	cell_t *a, *b;

	check(args && args->cdr,"too few arguments to eq");
	check(!args->cdr->cdr,"too many arguments to eq");

	a = eval(env,args->car);
	args = args->cdr;
	b = eval(env,args->car);

	if(!a || !b)
		return a || b ? NULL : sym_t;

	if(a == b)
		return sym_t;

	if(cell_type(a) != cell_type(b))
		return NULL;

	switch(cell_type(a)) {
	case VAL_SYM: return a->sym == b->sym ? sym_t : NULL;
	case VAL_I64: return a->i64 == b->i64 ? sym_t : NULL;
	case VAL_DBL: return a->dbl == b->dbl ? sym_t : NULL;
	case VAL_CHR: return a->chr == b->chr ? sym_t : NULL;
	case VAL_FCN: return a->fcn == b->fcn ? sym_t : NULL;

	case VAL_STR:
		return a->i64 == b->i64 && memcmp(a->data,b->data,a->i64) == 0
			? sym_t : NULL;

	case VAL_LBA:
		return memcmp(cell_lba(a),cell_lba(b),sizeof(lambda_t)) == 0
			? sym_t : NULL;

	case VAL_LST:
		return NULL;

	default:
		error("unhandled value in eq, type %i",cell_type(a));
		return NULL;
	}
}

static cell_t *eval_builtin(env_t *env, cell_t *args) {
	check(!args->cdr,"too many arguments to eval");

	return args ? eval(env,eval(env,args->car)) : NULL;
}

static cell_t *gensym(env_t *env, cell_t *args) {
	static int counter = 0;

	char *str;

	check(!args,"too many arguments to gensym");

	str = malloc(1 + 5 + 1); // TODO: GC this
	sprintf(str,"G%05i",counter++%100000);

	return cell_cons_t(VAL_SYM,str);
}

static cell_t *lambda(env_t *env, cell_t *args) {
	cell_t *arg;
	lambda_t lamb;

	check(args,"missing lambda parameter list");

	// Set up the lambda
	lamb.ismacro = false;
	lamb.env = env_ref(env);
	lamb.args = args->car;
	lamb.body = args->cdr;

	return cell_cons_t(VAL_LBA,&lamb);
}

static cell_t *macro(env_t *env, cell_t *args) {
	lambda_t mac;

	check(args,"missing macro parameter list");

	// Set up the macto
	mac.ismacro = true;
	mac.env = env_ref(env);
	mac.args = args->car;
	mac.body = args->cdr;

	return cell_cons_t(VAL_LBA,&mac);
}

static cell_t *macroexpand(env_t *env, cell_t *args) {
	cell_t *expr, *op;

	check(args && !args->cdr,
		"incorrect number of arguments to macroexpand");

	expr = eval(env,args->car);

	while(true) {
		if(cell_is_atom(expr))
			return expr;

		op = eval(env,expr->car);
		if(!op || cell_type(op) != VAL_LBA || !cell_lba(op)->ismacro)
			return expr;

		expr = eval_lambda(NULL,cell_lba(op),expr->cdr);
	}

	return expr;
}

static cell_t *macroexpand_1(env_t *env, cell_t *args) {
	cell_t *expr, *op;

	check(args && !args->cdr,
		"incorrect number of arguments to macroexpand-1");

	expr = eval(env,args->car);

	if(cell_is_atom(expr))
		return expr;

	op = eval(env,expr->car);
	if(!op || cell_type(op) != VAL_LBA || !cell_lba(op)->ismacro)
		return expr;

	return eval_lambda(NULL,cell_lba(op),expr->cdr);
}

static void print_cell(cell_t *args) {
	if(!args)
		printf("nil");
	else switch(cell_type(args)) {
	case VAL_SYM: printf("%s",args->sym);                        break;
	case VAL_I64: printf("%lli",args->i64);                      break;
	case VAL_DBL: printf("%f",args->dbl);                        break;
	case VAL_CHR: printf("%c",args->chr);                        break;
	case VAL_STR: printf("\"%.*s\"",(int) args->i64,args->data); break;
	case VAL_FCN: printf("<%p>",args->fcn);                      break;
	case VAL_LBA: printf("<%s>",cell_lba(args)->ismacro
		? "macro" : "lambda");                               break;

	case VAL_NIL:
	default:
		assert(cell_is_list(args));

		putchar('(');
		do {
			if(cell_is_list(args)) {
				print_cell(args->car);
				if(args->cdr)
					putchar(' ');
			} else {
				printf(". ");
				print_cell(args);
				break;
			}
		} while(args = args->cdr);
		putchar(')');
		break;
	}
}

static cell_t *print(env_t *env, cell_t *args) {
	for(; args; args = args->cdr)
		print_cell(eval(env,args->car));

	return NULL;
}

static cell_t *quasiquote_unquote(env_t *env, cell_t *sexp, bool *splice) {
	bool subsplice;
	cell_t *cell, *head, **tail;

	// Default
	if(splice)
		*splice = false;

	// atom
	if(cell_is_atom(sexp))
		return sexp;

	// ,sexp
	if(cell_type(sexp->car) == VAL_SYM
		&& sexp->car->sym == str_unquote) {
		sexp = sexp->cdr;

		check(sexp,"too few arguments to unquote");
		check(!sexp->cdr,"too many arguments to unquote");

		return eval(env,sexp->car);
	}

	// ,@sexp
	if(cell_type(sexp->car) == VAL_SYM
		&& sexp->car->sym == str_unquote_splicing) {
		sexp = sexp->cdr;

		check(splice,"syntax `,@sexp is undefined");
		check(sexp,"too few arguments to unquote-splicing");
		check(!sexp->cdr,"too many arguments to unquote-splicing");

		*splice = true;

		return eval(env,sexp->car);
	}

	// (... ,sexp ... ,@sexp ...)
	for(head = sexp, tail = &head; sexp; sexp = sexp->cdr) {
		cell = quasiquote_unquote(env,sexp->car,&subsplice);

		if(subsplice)
			*tail = cell;
		else *tail = cell_cons(cell,NULL);

		for(; *tail; tail = &(*tail)->cdr);
	}

	return head;
}

static cell_t *quasiquote(env_t *env, cell_t *args) {
	check(args,"too few arguments to quasiquote");
	check(!args->cdr,"too many arguments to quasiquote");

	return quasiquote_unquote(env,args->car,NULL);
}

static cell_t *quote(env_t *env, cell_t *args) {
	check(args,"too few arguments to quote");
	check(!args->cdr,"too many arguments to quote");

	return args->car;
}

static cell_t *assign(env_t *env, cell_t *args) {
	cell_t *sym, *val;

	check(args && args->cdr,"too few arguments to =");
	check(!args->cdr->cdr,"too many arguments to =");

	// The symbol to assign to
	sym = args->car;
	check(sym && cell_type(sym) == VAL_SYM,
		"first argument to = not a symbol");

	// The value to assign
	args = args->cdr;
	val = eval(env,args->car);

	// Assign it!
	env_set(env,sym->sym,val,false);

	return val;
}

static cell_t *add(env_t *env, cell_t *args) {
	cell_t *r, *x;
	double dbl, xdbl;
	int64_t i64, xi64;
	enum cell_type type;

	dbl = i64 = 0;
	type = VAL_NIL;
	for(; args; args = args->cdr) {
		x = eval(env,args->car);

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
	for(; args; args = args->cdr) {
		x = eval(env,args->car);

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
		{"append",       append},
		{"atom",         atom},
		{"car",          car},
		{"cdr",          cdr},
		{"cond",         cond},
		{"cons",         cons},
		{"eq",           eq},
		{"eval",         eval_builtin},
		{"gensym",       gensym},
		{"lambda",       lambda},
		{"macro",        macro},
		{"macroexpand",  macroexpand},
		{"macroexpand-1",macroexpand_1},
		{"print",        print},
		{"quasiquote",   quasiquote},
		{"quote",        quote},
		{"=",            assign},
		{"+",            add},
		{"-",            sub},
		{NULL, NULL}
	};

	// Cache important symbols
	str_t = cell_str_intern("t",1);
	str_unquote = cell_str_intern("unquote",7);
	str_unquote_splicing = cell_str_intern("unquote-splicing",16);

	// Canonical truth symbol
	sym_t = cell_cons_t(VAL_SYM,str_t);
	env_set(env,str_t,sym_t,true);

	// Builtin operators
	for(int i = 0; funcs[i].name; i++)
		env_set(env,cell_str_intern(funcs[i].name,
			strlen(funcs[i].name)),
			cell_cons_t(VAL_FCN,funcs[i].func),true);
}

