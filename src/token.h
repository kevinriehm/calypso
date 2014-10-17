#ifndef TOKEN_H
#define TOKEN_H

#include <stdbool.h>
#include <stdio.h>

typedef struct stream stream_t;

typedef union token_value {
	char chr;
	double dbl;
	int64_t i64;

	struct {
		char *str;
		int64_t len;
	};
} token_value_t;


stream_t *stream_cons_f(FILE *);
void stream_free(stream_t *);

bool stream_interactive(stream_t *);

int token_next(stream_t *, token_value_t *);

#endif

