#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <unistd.h>

#include "grammar.h"
#include "token.h"
#include "util.h"

%%{
	machine scanner;

	write data;
}%%

struct stream {
	FILE *f;

	bool interactive;

	char *buf;
	unsigned buflen;

	int act, cs;
	char *eof, *p, *pe, *te, *ts;

	unsigned lineno;

	bool tokinited;
};

stream_t *stream_cons_f(FILE *f) {
	stream_t *s;

	s = malloc(sizeof *s);
	assert(s);
	s->f = f;
	s->interactive = isatty(fileno(f));
	s->buflen = 10;
	s->buf = malloc(s->buflen + 1);
	s->eof = NULL;
	s->p = s->pe = s->buf;
	s->tokinited = false;

	return s;
}

void stream_free(stream_t *s) {
	free(s->buf);
	free(s);
}

bool stream_interactive(stream_t *s) {
	return s->interactive;
}

static void fill(stream_t *s) {
	char *newbuf;
	int c, offset;

	// Sanity check
	if(s->eof == s->pe)
		return;

	// Shift everything to the front of the buffer
	if(s->ts && (offset = s->ts - s->buf)) {
		memmove(s->buf,s->ts,s->pe - s->ts);
		s->p  -= offset;
		s->pe -= offset;
		s->te -= offset;
		s->ts -= offset;
	}

	// Do we still need more room?
	if(s->pe - s->buf + 1 > s->buflen) {
		s->buflen++;
		newbuf = realloc(s->buf,s->buflen + 1);
		s->p   = newbuf + (s->p  - s->buf);
		s->pe  = newbuf + (s->pe - s->buf);
		s->te  = newbuf + (s->te - s->buf);
		s->ts  = newbuf + (s->ts - s->buf);
		s->buf = newbuf;
	}

	// Actually get the data
	if(c = getc(s->f), c < 0)
		s->eof = s->pe;
	else *s->pe++ = c;

	// Safety check
	*s->pe = '\0';
}

int token_next(stream_t *s, token_value_t *val) {
	int ret;

	if(!s->tokinited) {
		s->tokinited = true;

		%%{
			write init;
		}%%
	}

	ret = 0;

refill:
	if(s->p == s->pe)
		fill(s);

	%%{
		variable act s->act;
		variable cs  s->cs;
		variable eof s->eof;
		variable p   s->p;
		variable pe  s->pe;
		variable te  s->te;
		variable ts  s->ts;

		S = [+\-]?;
		D = [0-9];
		E = [Ee] S D+;
		H = '0x' [0-9a-fA-F]+;
		I = '0' | [1-9] D*;
		O = '0' [0-7]+;

		scanner := |*
			[ \t]+ => { ret = SPACE; fbreak; };
			'\n'   => { s->lineno++; ret = NEWLINE; fbreak; };

			'(' => { ret = LPAREN; fbreak; };
			')' => { ret = RPAREN; fbreak; };
			'\'' => { ret = QUOTE; fbreak; };

			S I | S H | S O => {
				val->i64 = strtoll(s->ts,NULL,0);
				ret = INTEGER;
				fbreak;
			};

			S I '.' D* E? |
			S '.' D+ E?   |
			S 'inf'       |
			S 'nan'    => {
				val->dbl = strtod(s->ts,NULL);
				ret = REAL;
				fbreak;
			};

			'\'' . '\'' => {
				val->chr = s->ts[1];
				ret = CHARACTER;
				fbreak;
			};

			'\'\\n\'' => {
				val->chr = '\n';
				ret = CHARACTER;
				fbreak;
			};

			'"' ([^"] | '\\"')* '"' => {
				val->str = strndup(s->ts + 1,
					s->te - s->ts - 2);
				ret = STRING;
				fbreak;
			};

			[a-zA-Z$_][a-zA-Z0-9$_\-]* |
			[=+\-]                  => {
				val->str = strndup(s->ts,s->te - s->ts);
				ret = SYMBOL;
				fbreak;
			};
		*|;

		write exec;
	}%%

	if(s->cs == %%{ write error; }%%) {
		error("scanner error");
	}

	if(!ret && s->eof != s->pe)
		goto refill;

	return ret;
}

