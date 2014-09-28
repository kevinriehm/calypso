%include {
#include <assert.h>

#include "cell.h"
#include "token.h"
}

%extra_argument { cell_t **root }

%token_type { token_value_t }

%type s_exp      { cell_t * }
%type s_exp_list { cell_t * }
%type atom       { cell_t * }

super_root ::= root.

root ::= .
root ::= root s_exp(S). { *root = S; }

s_exp(R) ::= opt_space atom(A). { R = A; }
s_exp(R) ::= opt_space LPAREN s_exp_list(L) RPAREN.
	{ R = L; }
s_exp(R) ::= opt_space QUOTE s_exp(S). {
		R = cell_cons(
			cell_cons_t(VAL_SYM,"quote"),
			cell_cons(S,NULL)
		);
	}

s_exp_list(R) ::= . { R = NULL; }
s_exp_list(R) ::= s_exp(S) s_exp_list(L). { R = cell_cons(S,L); }

atom(A) ::= INTEGER(I).   { A = cell_cons_t(VAL_I64,I.i64); }
atom(A) ::= REAL(R).      { A = cell_cons_t(VAL_DBL,R.dbl); }
atom(A) ::= CHARACTER(C). { A = cell_cons_t(VAL_CHR,C.chr); }
atom(A) ::= STRING(S).    { A = cell_cons_t(VAL_STR,S.str); }
atom(A) ::= SYMBOL(S).    { A = cell_cons_t(VAL_SYM,S.str); }

opt_space ::= .
opt_space ::= opt_space SPACE.
opt_space ::= opt_space NEWLINE.

