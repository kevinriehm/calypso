%{
#include <stdint.h>
#include <stdlib.h>

#include "cell.h"
#include "util.h"

cell_t *parseroot;

int yyerror(const char *);
int yylex(void);
%}

%union {
	int64_t i64;

	double dbl;

	char chr;
	char *str;

	cell_t *cell;
}

%token <dbl> REAL
%token <i64> INTEGER
%token <chr> CHARACTER
%token <str> STRING SYMBOL

%type <cell> s_exp s_exp_list atom

%%

root: s_exp { parseroot = $1; YYACCEPT; }
    ;

s_exp: atom
     | '(' s_exp_list ')' { $$ = $2; }
     | '\'' '(' s_exp_list ')' {
		$$ = cell_cons(
			cell_cons_t(VAL_SYM,"quote"),
			cell_cons($3,NULL)
		);
	}
     ;

s_exp_list: { $$ = NULL; }
          | s_exp s_exp_list { $$ = cell_cons($1,$2); }
          ;

atom: INTEGER   { $$ = cell_cons_t(VAL_I64,$1); }
    | REAL      { $$ = cell_cons_t(VAL_DBL,$1); }
    | CHARACTER { $$ = cell_cons_t(VAL_CHR,$1); }
    | STRING    { $$ = cell_cons_t(VAL_STR,$1); }
    | SYMBOL    { $$ = cell_cons_t(VAL_SYM,$1); }
    ;

%%

int yyerror(const char *msg) {
	error("%s",msg);
}

