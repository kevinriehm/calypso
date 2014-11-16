%include {
#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include "cell.h"
#include "token.h"
#include "util.h"

static cell_t *wrap(char *first, cell_t *cell) {
	return cell_cons(
		cell_cons_t(VAL_SYM,cell_str_intern(first,strlen(first))),
		cell_cons(cell,NULL)
	);
}
}

%extra_argument { cell_t **root }

%token_prefix TOK_

%token_type { token_value_t }

%type s_exp         { cell_t * }
%type s_exp_list    { cell_t * }
%type bq_s_exp      { cell_t * }
%type bq_s_exp_list { cell_t * }
%type atom          { cell_t * }

root ::= cmd.

cmd ::= .
cmd ::= cmd error. { error("syntax error"); }
cmd ::= cmd s_exp(S). { *root = S; }

s_exp(R) ::= atom(A). { R = A; }
s_exp(R) ::= LPAREN s_exp_list(L) RPAREN. { R = L; }
s_exp(R) ::= LPAREN s_exp_list(CAR) PERIOD s_exp(CDR) RPAREN. {
		cell_t *tail;

		R = CAR ? CAR : cell_cons(NULL,NULL);

		for(tail = R; tail->cdr; tail = tail->cdr);
		tail->cdr = CDR;
	}
s_exp(R) ::= QUOTE s_exp(S). { R = wrap("quote",S); }
s_exp(R) ::= BQUOTE s_exp(S). { R = wrap("quasiquote",S); }
s_exp(R) ::= COMMA s_exp(S). { R = wrap("unquote",S); }
s_exp(R) ::= COMMA AT s_exp(S). { R = wrap("unquote-splicing",S); }

s_exp_list(R) ::= . { R = NULL; }
s_exp_list(R) ::= s_exp(S) s_exp_list(L). { R = cell_cons(S,L); }

atom(A) ::= INTEGER(I).   { A = cell_cons_t(VAL_I64,I.i64); }
atom(A) ::= REAL(R).      { A = cell_cons_t(VAL_DBL,R.dbl); }
atom(A) ::= CHARACTER(C). { A = cell_cons_t(VAL_CHR,C.chr); }
atom(A) ::= STRING(S).    { A = cell_cons_t(VAL_STR,S.str,S.len); }
atom(A) ::= SYMBOL(S).    {
		A = cell_cons_t(VAL_SYM,cell_str_intern(S.str,S.len));
	}

