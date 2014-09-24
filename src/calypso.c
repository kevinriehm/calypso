#include <assert.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cell.h"

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

cell_t *eval(cell_t *sexp) {
	return sexp;
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

void run_file(FILE *in) {
	cell_t *sexp;

	while(readf(in,&sexp)) {
		print(eval(sexp));
		putchar('\n');
	}
}

int main(int argc, char **argv) {
	int i;
	FILE *in;

	if(argc > 1) {
		for(i = 1; i < argc; i++) {
			if(strcmp(argv[i],"-") == 0)
				run_file(stdin);
			else {
				if(in = fopen(argv[i],"r"))
					run_file(in);
				else die("cannot open '%s'",argv[i]);
			}
		}
	} else run_file(stdin);

	return 0;
}

