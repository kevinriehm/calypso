#include <assert.h>
#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "htable.h"
#include "util.h"

#define THRESH_GROW   0.7
#define THRESH_SHRINK 0.3

#define RESIZE_FACTOR 2

#define HASH_SEED 0xb6871303

#define HASH(key, keylen, cap) (murmur3_32(HASH_SEED,(key),(keylen))%(cap))

typedef struct hentry {
	void *key;
	uint32_t keylen;

	hvalue_t val;

	struct hentry *next;
} hentry_t;

struct htable {
	uint32_t cap;
	uint32_t mincap;

	uint32_t nentries;
	hentry_t **entries;
};

static uint32_t murmur3_32(uint32_t seed, void *key, size_t keylen) {
	static const uint32_t r1 = 15, r2 = 13;
	static const uint32_t m = 5, n = 0xe6546b64;
	static const uint32_t c1 = 0xcc9e2d51, c2 = 0x1b873593;

	size_t i;
	uint32_t k;

	for(i = 0; keylen - i > 3; i += 4) {
		k = *(uint32_t *) (key + i);

		k *= c1;
		k = (k << r1) | (k >> 32 - r1);
		k *= c2;

		seed ^= k;
		seed = (seed << r2) | (seed >> 32 - r2);
		seed = seed*m + n;
	}

	for(k = 0; i < keylen; i++)
		k |= *(uint8_t *) (key + i) << 8*(3 - keylen + i);

	k *= c1;
	k = (k << r1) | (k >> 32 - r1);
	k *= c2;

	seed ^= k;

	seed ^= keylen;

	seed ^= seed >> 16;
	seed *= 0x85ebca6b;
	seed ^= seed >> 13;
	seed *= 0xc2b2ae35;
	seed ^= seed >> 16;

	return seed;
}

static void htable_resize(htable_t *tab, uint32_t cap) {
	uint32_t index;
	hentry_t *entry, **entries, *next;

	// Obey the hashtable's creator
	if(cap < tab->mincap)
		cap = tab->mincap;

	// Sanity check
	if(cap == tab->cap)
		return;

	entries = calloc(cap,sizeof *entries);

	for(int i = 0; i < tab->cap; i++) {
		for(entry = tab->entries[i]; entry; entry = next) {
			index = HASH(entry->key,entry->keylen,cap);

			next = entry->next;
			entry->next = entries[index];
			entries[index] = entry;
		}
	}

	free(tab->entries);

	tab->cap = cap;
	tab->entries = entries;
}

htable_t *htable_cons(uint32_t mincap) {
	htable_t *tab;

	tab = malloc(sizeof *tab);

	tab->cap = 1 << (int) (log2((mincap ? mincap : 0x10) - 1) + 1);
	tab->mincap = mincap;
	tab->nentries = 0;
	tab->entries = calloc(tab->cap,sizeof *tab->entries);

	return tab;
}

void htable_free(htable_t *tab) {
	hentry_t *entry;

	// Free our copies of the keys
	for(int i = 0; i < tab->cap; i++) {
		for(entry = tab->entries[i]; entry; entry = entry->next) {
			free(entry->key);
			free(entry);
		}
	}

	free(tab->entries);
	free(tab);
}

void htable_insert(htable_t *tab, void *key, size_t keylen, hvalue_t val) {
	uint32_t index;
	hentry_t *entry;

	index = HASH(key,keylen,tab->cap);

	for(entry = tab->entries[index]; entry; entry = entry->next) {
		if(entry->keylen == keylen
			&& memcmp(entry->key,key,keylen) == 0) {
			// key is already in tab
			entry->val = val;
			return;
		}
	}

	// key isn't in tab (yet)
	entry = malloc(sizeof *entry);
	entry->key = memdup(key,keylen + 1);
	((char *) entry->key)[keylen] = '\0';
	entry->keylen = keylen;
	entry->val = val;
	entry->next = tab->entries[index];

	tab->entries[index] = entry;

	// Too many entries?
	if(++tab->nentries > THRESH_GROW*tab->cap)
		htable_resize(tab,RESIZE_FACTOR*tab->cap);
}

bool htable_lookup(htable_t *tab, void *key, size_t keylen, hvalue_t *val) {
	uint32_t index;
	hentry_t *entry;

	index = HASH(key,keylen,tab->cap);

	for(entry = tab->entries[index]; entry; entry = entry->next) {
		if(entry->keylen == keylen
			&& memcmp(entry->key,key,keylen) == 0) {
			if(val) *val = entry->val;
			return true;
		}
	}

	// key isn't in tab
	return false;
}

void htable_remove(htable_t *tab, void *key, size_t keylen) {
	uint32_t index;
	hentry_t *entry, *prev;

	index = HASH(key,keylen,tab->cap);

	for(prev = NULL, entry = tab->entries[index]; entry;
		prev = entry, entry = entry->next) {
		if(entry->keylen == keylen
			&& memcmp(entry->key,key,keylen) == 0) {
			if(prev)
				prev->next = entry->next;
			else tab->entries[index] = entry->next;

			free(entry->key);
			free(entry);

			// Too few entries?
			if(--tab->nentries > THRESH_SHRINK*tab->cap)
				htable_resize(tab,tab->cap/RESIZE_FACTOR);

			return;
		}
	}

	// key isn't in tab
}

char *htable_intern(htable_t *tab, void *key, size_t keylen) {
	uint32_t index;
	hentry_t *entry;

	while(true) {
		index = HASH(key,keylen,tab->cap);

		for(entry = tab->entries[index]; entry; entry = entry->next)
			if(entry->keylen == keylen
				&& memcmp(key,entry->key,keylen) == 0)
				return entry->key;

		htable_insert(tab,key,keylen,(hvalue_t) { .p = NULL });
	}
}

