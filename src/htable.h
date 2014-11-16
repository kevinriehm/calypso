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

void htable_insert(htable_t *, void *, size_t, hvalue_t);
bool htable_lookup(htable_t *, void *, size_t, hvalue_t *);
void htable_remove(htable_t *, void *, size_t);

char *htable_intern(htable_t *, void *, size_t);

#endif

