%{
#include <stdint.h>

#include "util.h"
%}

%union {
	double dbl;

	int64_t  i64;

	char chr;
	char *str;
}

%token <dbl> REAL
%token <i64> INTEGER
%token <chr> CHARACTER
%token <str> STRING SYMBOL

%token SPACE

%%

s_exp: atom
     | '(' s_exp_list ')'
     | '\'' '(' s_exp_list ')';

s_exp_list:
          | SPACE
          | s_exp SPACE s_exp_list;

atom: INTEGER
    | REAL
    | STRING
    | SYMBOL;

%%

int yyerror(const char *msg) {
	error("%s",msg);
}

