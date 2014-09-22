#ifndef HTABLE_H
#define HTABLE_H

#include <stdbool.h>
#include <stdint.h>

typedef struct htable htable_t;

typedef union hvalue {
	void *p;
	uint32_t i;
} hvalue_t;

htable_t *htable_cons(uint32_t);
void htable_free(htable_t *);

void htable_insert(htable_t *, char *, hvalue_t);
bool htable_lookup(htable_t *, char *, hvalue_t *);
void htable_remove(htable_t *, char *);

#endif

