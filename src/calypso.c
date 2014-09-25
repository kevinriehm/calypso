#include <assert.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cell.h"
#include "env.h"

typedef struct string {
	char *s;
	unsigned cap, len;
} string_t;

extern FILE *yyin;
extern cell_t *parseroot;

int yyparse(void);

void error(char *str, ...) {
	va_list ap;

	fprintf(stderr,"error: ");

	va_start(ap,str);
	vfprintf(stderr,str,ap);
	va_end(ap);

	fputc('\n',stderr);
}

void die(char *str, ...) {
	va_list ap;

	fprintf(stderr,"error: ");

	va_start(ap,str);
	vfprintf(stderr,str,ap);
	va_end(ap);

	fputc('\n',stderr);

	exit(EXIT_FAILURE);
}

void str_init(string_t *str) {
	str->s = NULL;
	str->cap = 0;
	str->len = 0;
}

void str_cat_c(string_t *str, char c) {
	if(++str->len > str->cap) {
		str->cap = 1.5*(str->cap + 1);
		str->s = realloc(str->s,str->cap + 1);
	}

	str->s[str->len - 1] = c;
	str->s[str->len] = '\0';
}

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

cell_t *add(env_t *env, cell_t *args) {
	double dbl, xdbl;
	int64_t i64, xi64;
	cell_t *r, *x;
	enum cell_type type;

	dbl = i64 = 0;
	type = VAL_NIL;
	for(; args; args = args->cdr.p) {
		x = eval(env,args->car.p);

		assert(x
			&& (x->car.type == VAL_I64 || x->car.type == VAL_DBL));

		switch(x->car.type) {
		case VAL_I64: xdbl = xi64 = x->cdr.i64; break;
		case VAL_DBL: xdbl = xi64 = x->cdr.dbl; break;
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

int main(int argc, char **argv) {
	int i;
	FILE *in;
	env_t *globals;

	globals = env_cons(NULL);

	env_set(globals,"+",cell_cons_t(VAL_FCN,add),true);

	if(argc > 1) {
		for(i = 1; i < argc; i++) {
			if(strcmp(argv[i],"-") == 0)
				run_file(globals,stdin);
			else {
				if(in = fopen(argv[i],"r"))
					run_file(globals,in);
				else die("cannot open '%s'",argv[i]);
			}
		}
	} else run_file(globals,stdin);

	return 0;
}

