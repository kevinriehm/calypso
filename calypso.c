#include <ctype.h>
#include <limits.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <regex.h>

#include "rbtree.h"

typedef struct string {
	char *s;
	unsigned cap, len;
} string_t;

typedef enum value_type {
	VAL_NIL,
	VAL_SYM,
	VAL_DBL,

	NUM_VAL_TYPES
} value_type_t;

typedef union value {
	struct cell *sym;
	double dbl;
} value_t;

typedef struct cell {
	union {
		struct cell *car;

		value_type_t type;
	};

	union {
		struct cell *cdr;

		value_t val;
	};
} cell_t;

typedef struct token {
	enum {
		// All char values are token types
		TOK_EOF = UCHAR_MAX + 1,
		TOK_ATOM
	} type;

	union {
		string_t str;
	};
} token_t;

regex_t renum;

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

void sym_bind(char *name, cell_t *sym) {
	
}

cell_t *sym_lookup(char *name) {
	return NULL;
}

cell_t *cell_alloc() {
	return malloc(sizeof(cell_t));
}

void readf_token(token_t *tok, FILE *f) {
	int c;

	while(c = getc(f), isspace(c));

	// No token at all?
	if(c == EOF) {
		tok->type = TOK_EOF;
		return;
	}

	// One character token?
	if(c == '(' || c == ')') {
		tok->type = c;
		return;
	}

	// Parse this as one of the value types
	tok->type = TOK_ATOM;
	do str_cat_c(&tok->str,c);
	while(c = getc(f), !isspace(c) && c != ')');

	// Put that last one back
	ungetc(c,f);
}

cell_t *readf(FILE *f) {
	bool inlist;
	token_t tok;
	cell_t *atom, *root, *sexp;

	inlist = false;

	str_init(&tok.str);

	root = sexp = NULL;

	while(true) {
		readf_token(&tok,f);

		switch(tok.type) {
		case '(':
			if(inlist) {
				ungetc('(',f);

				if(sexp) sexp = sexp->cdr = cell_alloc();
				else root = sexp = cell_alloc();

				sexp->car = readf(f);
			} else inlist = true;
			break;

		case ')':
			if(inlist) {
				if(!sexp) {
					root = sexp = cell_alloc();
					sexp->car = VAL_NIL;
				}

				sexp->cdr = NULL;
			} else die("unexpected ')'");
			return root;

		case TOK_EOF:
			if(inlist) die("expected ')', found EOF");
			return NULL;

		case TOK_ATOM:
			if(atom = sym_lookup(tok.str.s), !atom) {
				atom = cell_alloc();
				atom->type = VAL_SYM;
				atom->val.sym = atom;

				sym_bind(tok.str.s,atom);
			}

			if(!inlist) return atom;

			if(sexp) sexp = sexp->cdr = cell_alloc();
			else root = sexp = cell_alloc();

			sexp->car = atom;
			break;

		default:
			die("unhandled token '%s', of type %i",tok.str,
				tok.type);
			break;
		}
	}
}

cell_t *eval(cell_t *sexp) {
	return sexp;
}

void print(cell_t *sexp) {
	switch(sexp->type) {
	case VAL_NIL:
		printf("nil");
		break;

	case VAL_SYM:
	case VAL_DBL:
		printf("atom");
		break;

	default:
		putchar('(');
		do {
			print(sexp->car);
			if(sexp->cdr) putchar(' ');
		} while(sexp = sexp->cdr);
		putchar(')');
		break;
	}
}

void run_file(FILE *in) {
	cell_t *sexp;

	while(sexp = readf(in)) {
		print(eval(sexp));
		putchar('\n');
	}
}

int main(int argc, char **argv) {
	int i;
	FILE *in;

	// Compile the regular expressions
	regcomp(&renum,"[+-]?([0-9]+\\.?|\\.)[0-9]*[df]?",
		REG_EXTENDED | REG_ICASE | REG_NOSUB);

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

	regfree(&renum);

	return 0;
}

