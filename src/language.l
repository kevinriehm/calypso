%{
#include "cell.h"
#include "repl.h"

#include "language.tab.h"
%}

INT 0|[1-9][0-9]*

%%

\  ; // Whitespace
\n lineno++;

[+-]?{INT}             |
[+-]?0[Xx][0-9a-fA-F]+ |
[+-]?0[0-7]+           {
	yylval.i64 = strtoll(yytext,NULL,0);
	return INTEGER;
}

[+-]?{INT}\.[0-9]*([Ee][0-9]+)? |
[+-]?\.[0-9]+([Ee][0-9]+)?      | 
[+-]?inf                        |
nan                             {
	yylval.dbl = strtod(yytext,NULL);
	return REAL;
}

'.' {
	yylval.chr = yytext[1];
	return CHARACTER;
}

\"([^"]|\\\")*\" {
	yylval.str = strndup(yytext + 1,yyleng - 2);
	return STRING;
}

[a-zA-Z$_][a-zA-Z0-9$_-]* |
=|\+|-                    {
	yylval.str = strdup(yytext);
	return SYMBOL;
}

. return yytext[0];

%%

int yywrap() {
	return 1;
}

