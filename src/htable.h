#ifndef HTABLE_H
#define HTABLE_H

#include <stdbool.h>
#include <stdint.h>

#include "mem.h"

typedef struct hvalue {
	gc_type_t type;

	union {
		void *p;
		uint32_t i;
	};
} hvalue_t;

typedef struct hentry {
	void *key;
	uint32_t keylen;

	hvalue_t val;

	struct hentry *next;
} hentry_t;

typedef struct htable {
	uint32_t cap;
	uint32_t mincap;

	uint32_t nentries;
	hentry_t **entries;
} htable_t;

htable_t *htable_cons(uint32_t);

void htable_insert(htable_t *, void *, size_t, hvalue_t);
bool htable_lookup(htable_t *, void *, size_t, hvalue_t *);
void htable_remove(htable_t *, void *, size_t);

char *htable_intern(htable_t *, void *, size_t);

#endif

