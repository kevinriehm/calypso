#ifndef TOKEN_H
#define TOKEN_H

#include <stdbool.h>
#include <stdio.h>

typedef struct stream stream_t;

typedef union token_value {
	int64_t i64;
	double dbl;

	char chr;
	char *str;
} token_value_t;


stream_t *stream_cons_f(FILE *);
void stream_free(stream_t *);

bool stream_interactive(stream_t *);

int token_next(stream_t *, token_value_t *);

#endif

