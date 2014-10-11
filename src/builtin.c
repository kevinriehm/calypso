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

	for(head = NULL, tail = &head; args; args = args->cdr.p) {
		val = eval(env,args->car.p);

		check(cell_is_list(val),"arguments to append must be lists");

		if(args->cdr.p) {
			for(; val; val = val->cdr.p) {
				*tail = cell_dup(val);
				tail = &(*tail)->cdr.p;
			}
		} else *tail = val;
	}

	return head;
}

static cell_t *atom(env_t *env, cell_t *args) {
	check(args && !args->cdr.p,"too many arguments to atom");

	return cell_is_atom(eval(env,args->car.p)) ? sym_t : NULL;
}

static cell_t *car(env_t *env, cell_t *args) {
	check(args && !args->cdr.p,"too many arguments to car");
	args = eval(env,args->car.p);

	check(args && cell_is_list(args),
		"argument to car must be a non-empty list");

	return args->car.p;
}

static cell_t *cdr(env_t *env, cell_t *args) {
	check(args && !args->cdr.p,
		"too many arguments to cdr");
	args = eval(env,args->car.p);

	check(args && cell_is_list(args),
		"argument to cdr must be a non-empty list");

	return args->cdr.p;
}

static cell_t *cond(env_t *env, cell_t *args) {
	cell_t *pair;

	for(; args; args = args->cdr.p) {
		pair = args->car.p;
		check(pair && pair->cdr.p && !pair->cdr.p->cdr.p,
			"argument to cond must be a pair");

		if(eval(env,pair->car.p))
			return eval(env,pair->cdr.p->car.p);
	}

	return NULL;
}

static cell_t *cons(env_t *env, cell_t *args) {
	cell_t *a, *b;

	check(args && args->cdr.p,"too few arguments to cons");
	check(!args->cdr.p->cdr.p,"too many arguments to cons");

	a = eval(env,args->car.p);
	args = args->cdr.p;
	b = eval(env,args->car.p);

	return cell_cons(a,b);
}

static cell_t *eq(env_t *env, cell_t *args) {
	cell_t *a, *b;

	check(args && args->cdr.p,"too few arguments to eq");
	check(!args->cdr.p->cdr.p,"too many arguments to eq");

	a = eval(env,args->car.p);
	args = args->cdr.p;
	b = eval(env,args->car.p);

	if(!a || !b)
		return a || b ? NULL : sym_t;

	if(a == b)
		return sym_t;

	if(a->car.type != b->car.type)
		return NULL;

	switch(a->car.type) {
	case VAL_SYM: return a->cdr.str == b->cdr.str ? sym_t : NULL;
	case VAL_I64: return a->cdr.i64 == b->cdr.i64 ? sym_t : NULL;
	case VAL_DBL: return a->cdr.dbl == b->cdr.dbl ? sym_t : NULL;
	case VAL_CHR: return a->cdr.chr == b->cdr.chr ? sym_t : NULL;
	case VAL_STR: return strcmp(a->cdr.str,b->cdr.str) == 0 ? sym_t : NULL;
	case VAL_FCN: return a->cdr.fcn == b->cdr.fcn ? sym_t : NULL;
	case VAL_LBA: return a->cdr.lba == b->cdr.lba ? sym_t : NULL;

	default:
		error("unhandled value in eq, type %i",a->car.type);
		return NULL;
	}
}

static cell_t *gensym(env_t *env, cell_t *args) {
	static int counter = 0;

	char *str;

	check(!args,"too many arguments to gensym");

	str = malloc(1 + 5 + 1);
	sprintf(str,"G%05i",counter++%100000);

	return cell_cons_t(VAL_SYM,str);
}

static cell_t *lambda(env_t *env, cell_t *args) {
	cell_t *arg;
	lambda_t *lamb;

	check(args,"missing lambda parameter list");

	// Check the argument list
	for(arg = args->car.p; arg; arg = arg->cdr.p)
		check(arg->car.p->car.type == VAL_SYM,
			"invalid symbol name in lambda parameter list");

	// Set up the lambda
	lamb = malloc(sizeof *lamb);
	assert(lamb);
	lamb->ismacro = false;
	lamb->env = env_ref(env);
	lamb->args = args->car.p;
	lamb->body = args->cdr.p;

	return cell_cons_t(VAL_LBA,lamb);
}

static cell_t *list(env_t *env, cell_t *args) {
	cell_t *head, **tail;

	head = NULL;
	tail = &head;

	for(; args; args = args->cdr.p) {
		*tail = cell_cons(eval(env,args->car.p),NULL);
		tail = &(*tail)->cdr.p;
	}

	return head;
}

static cell_t *macro(env_t *env, cell_t *args) {
	lambda_t *mac;

	check(args,"missing macro parameter list");

	// Set up the macto
	mac = malloc(sizeof *mac);
	assert(mac);
	mac->ismacro = true;
	mac->env = env_ref(env);
	mac->args = args->car.p;
	mac->body = args->cdr.p;

	return cell_cons_t(VAL_LBA,mac);
}

static void print_cell(cell_t *args) {
	if(!args)
		printf("nil");
	else switch(args->car.type) {
	case VAL_SYM: printf("%s",args->cdr.str);   break;
	case VAL_I64: printf("%lli",args->cdr.i64); break;
	case VAL_DBL: printf("%f",args->cdr.dbl);   break;
	case VAL_CHR: printf("%c",args->cdr.chr);   break;
	case VAL_STR: printf("%s",args->cdr.str);   break;
	case VAL_FCN: printf("<%p>",args->cdr.fcn); break;
	case VAL_LBA: printf("<%s>",args->cdr.lba->ismacro
		? "macro" : "lambda");              break;

	case VAL_NIL:
	default:
		assert(cell_is_list(args));

		putchar('(');
		do {
			if(cell_is_list(args)) {
				print_cell(args->car.p);
				if(args->cdr.p)
					putchar(' ');
			} else {
				printf(". ");
				print_cell(args);
				break;
			}
		} while(args = args->cdr.p);
		putchar(')');
		break;
	}
}

static cell_t *print(env_t *env, cell_t *args) {
	for(; args; args = args->cdr.p)
		print_cell(eval(env,args->car.p));

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
	if(sexp->car.p->car.type == VAL_SYM
		&& sexp->car.p->cdr.str == str_unquote) {
		sexp = sexp->cdr.p;

		check(sexp,"too few arguments to unquote");
		check(!sexp->cdr.p,"too many arguments to unquote");

		return eval(env,sexp->car.p);
	}

	// ,@sexp
	if(sexp->car.p->car.type == VAL_SYM
		&& sexp->car.p->cdr.str == str_unquote_splicing) {
		sexp = sexp->cdr.p;

		check(splice,"syntax `,@sexp is undefined");
		check(sexp,"too few arguments to unquote-splicing");
		check(!sexp->cdr.p,"too many arguments to unquote-splicing");

		*splice = true;

		return eval(env,sexp->car.p);
	}

	// (... ,sexp ... ,@sexp ...)
	for(head = sexp, tail = &head; sexp; sexp = sexp->cdr.p) {
		cell = quasiquote_unquote(env,sexp->car.p,&subsplice);

		if(subsplice)
			*tail = cell;
		else *tail = cell_cons(cell,NULL);

		for(; *tail; tail = &(*tail)->cdr.p);
	}

	return head;
}

static cell_t *quasiquote(env_t *env, cell_t *args) {
	check(args,"too few arguments to quasiquote");
	check(!args->cdr.p,"too many arguments to quasiquote");

	return quasiquote_unquote(env,args->car.p,NULL);
}

static cell_t *quote(env_t *env, cell_t *args) {
	check(args,"too few arguments to quote");
	check(!args->cdr.p,"too many arguments to quote");

	return args->car.p;
}

static cell_t *assign(env_t *env, cell_t *args) {
	cell_t *sym, *val;

	check(args && args->cdr.p,"too few arguments to =");
	check(!args->cdr.p->cdr.p,"too many arguments to =");

	// The symbol to assign to
	sym = args->car.p;
	check(sym && sym->car.type == VAL_SYM,
		"first argument to = not a symbol");

	// The value to assign
	args = args->cdr.p;
	val = eval(env,args->car.p);

	// Assign it!
	env_set(env,sym->cdr.str,val,false);

	return val;
}

static cell_t *add(env_t *env, cell_t *args) {
	cell_t *r, *x;
	double dbl, xdbl;
	int64_t i64, xi64;
	enum cell_type type;

	dbl = i64 = 0;
	type = VAL_NIL;
	for(; args; args = args->cdr.p) {
		x = eval(env,args->car.p);

		check(x && (x->car.type == VAL_I64 || x->car.type == VAL_DBL),
			"argument to + not a number");

		switch(x->car.type) {
		case VAL_I64: xdbl = xi64 = x->cdr.i64; break;
		case VAL_DBL: xi64 = xdbl = x->cdr.dbl; break;
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

static cell_t *sub(env_t *env, cell_t *args) {
	cell_t *r, *x;
	double dbl, xdbl;
	int64_t i64, xi64;
	enum cell_type type;

	dbl = i64 = 0;
	type = VAL_NIL;
	for(; args; args = args->cdr.p) {
		x = eval(env,args->car.p);

		check(x && (x->car.type == VAL_I64 || x->car.type == VAL_DBL),
			"argument to - not a number");

		switch(x->car.type) {
		case VAL_I64: xdbl = xi64 = x->cdr.i64; break;
		case VAL_DBL: xi64 = xdbl = x->cdr.dbl; break;
		}

sub_x:
		switch(type) {
		case VAL_NIL:
			type = x->car.type;

			if(args->cdr.p) {
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
		{"append",    append},
		{"atom",      atom},
		{"car",       car},
		{"cdr",       cdr},
		{"cond",      cond},
		{"cons",      cons},
		{"eq",        eq},
		{"gensym",    gensym},
		{"lambda",    lambda},
		{"list",      list},
		{"macro",     macro},
		{"print",     print},
		{"quasiquote",quasiquote},
		{"quote",     quote},
		{"=",         assign},
		{"+",         add},
		{"-",         sub},
		{NULL, NULL}
	};

	// Cache important symbols
	str_t = cell_str_intern("t");
	str_unquote = cell_str_intern("unquote");
	str_unquote_splicing = cell_str_intern("unquote-splicing");

	// Canonical truth symbol
	sym_t = cell_cons_t(VAL_SYM,str_t);
	env_set(env,str_t,sym_t,true);

	// Builtin operators
	for(int i = 0; funcs[i].name; i++)
		env_set(env,cell_str_intern(funcs[i].name),
			cell_cons_t(VAL_FCN,funcs[i].func),true);
}

