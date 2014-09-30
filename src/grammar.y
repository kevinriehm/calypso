%include {
#include <assert.h>

#include "cell.h"
#include "check.h"
#include "token.h"
#include "util.h"

static cell_t *comma = NULL, *commat = NULL;

static cell_t *listify(cell_t *cell) {
	return cell_cons(
		cell_cons_t(VAL_SYM,"list"),
		cell_cons(cell,NULL)
	);
}

static cell_t *quotify(cell_t *cell) {
	return cell_cons(
		cell_cons_t(VAL_SYM,"quote"),
		cell_cons(cell,NULL)
	);
}

// Recurse into cell to handle ',' and ',@' expressions
static cell_t *backquotify(cell_t *cell) {
	cell_t *head, **tail;

	// `a
	if(cell_is_atom(cell))
		return quotify(cell);

	// `,a `,(a b c)
	if(cell->car.p == comma) {
		if(cell_is_atom(cell->cdr.p))
			return cell->cdr.p;
		else return backquotify(cell->cdr.p);
	}	

	// `,@(a b c)
	if(cell->car.p == commat) {
		if(cell_is_list(cell->cdr.p))
			return cell->cdr.p;
		else {
			error("syntax '`,@atom' is undefined");
			return NULL;
		}
	}

	// `(a b c)
	head = cell_cons(cell_cons_t(VAL_SYM,"append"),NULL);
	for(tail = &head->cdr.p; cell; cell = cell->cdr.p) {
		if(cell->car.p && cell->car.p->car.p == comma)
			*tail = cell_cons(listify(cell->car.p->cdr.p),NULL);
		else if(cell->car.p && cell->car.p->car.p == commat)
			*tail = cell_cons(cell->car.p->cdr.p,NULL);
		else *tail = cell_cons(listify(backquotify(cell->car.p)),NULL);

		tail = &(*tail)->cdr.p;
	}

	return head;
}

void grammar_init() {
	if(!comma)
		comma = cell_cons_t(VAL_SYM,",");
	if(!commat)
		commat = cell_cons_t(VAL_SYM,",@");
}
}

%extra_argument { cell_t **root }

%token_type { token_value_t }

%type s_exp         { cell_t * }
%type s_exp_list    { cell_t * }
%type bq_s_exp      { cell_t * }
%type bq_s_exp_list { cell_t * }
%type atom          { cell_t * }

super_root ::= root.

root ::= .
root ::= root s_exp(S). { *root = S; }
root ::= root error. { printf("error!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!\n"); }

s_exp(R) ::= atom(A). { R = A; }
s_exp(R) ::= LPAREN s_exp_list(L) RPAREN. { R = L; }
s_exp(R) ::= QUOTE s_exp(S). { R = quotify(S); }
s_exp(R) ::= BQUOTE s_exp(S). { R = quotify(S); }
s_exp(R) ::= BQUOTE bq_s_exp(B). { R = backquotify(B); }

s_exp(R) ::= NEWLINE s_exp(S). { R = S; }

s_exp_list(R) ::= . { R = NULL; }
s_exp_list(R) ::= s_exp(S) s_exp_list(L). { R = cell_cons(S,L); }

bq_s_exp(R) ::= COMMA s_exp(S). { R = cell_cons(comma,S); }
bq_s_exp(R) ::= COMMA AT s_exp(S). { R = cell_cons(commat,S); }
bq_s_exp(R) ::= LPAREN bq_s_exp_list(L) RPAREN. { R = L; }

bq_s_exp_list(R) ::= s_exp(S) bq_s_exp_list(L). { R = cell_cons(S,L); }
bq_s_exp_list(R) ::= bq_s_exp(B) s_exp_list(L). { R = cell_cons(B,L); }
bq_s_exp_list(R) ::= bq_s_exp(S) bq_s_exp_list(L). { R = cell_cons(S,L); }

atom(A) ::= INTEGER(I).   { A = cell_cons_t(VAL_I64,I.i64); }
atom(A) ::= REAL(R).      { A = cell_cons_t(VAL_DBL,R.dbl); }
atom(A) ::= CHARACTER(C). { A = cell_cons_t(VAL_CHR,C.chr); }
atom(A) ::= STRING(S).    { A = cell_cons_t(VAL_STR,S.str); }
atom(A) ::= SYMBOL(S).    { A = cell_cons_t(VAL_SYM,S.str); }

//opt_space ::= .
//opt_space ::= NEWLINE opt_space.

