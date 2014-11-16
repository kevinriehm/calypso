#include <assert.h>
#include <stdalign.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

#include "util.h"

#define ARENA_SIZE (1 << 20)
#define ARENA_MAX_ALLOC 1024

#define ARENA_NEW 0x00000001ul

typedef struct arena {
	struct arena *next;

	size_t size;
	uint32_t flags;
	struct free_block {
		void *next;
	} *freelist;

	uint8_t data[];
} arena_t;

static void *small_alloc(arena_t **arenas, size_t size) {
	void *p;
	arena_t *arena;
	size_t blocksoff, nblocks;

	assert(size != 0 && !(size & size - 1));
	assert(size >= sizeof(struct free_block));
	assert(size >= alignof(max_align_t));

	// Search the existing arenas first
	for(arena = *arenas; arena && !arena->freelist; arena = arena->next);

	// Did we find one?
	if(arena) {
		p = arena->freelist;
		if(arena->flags & ARENA_NEW) {
			arena->freelist = (struct free_block *)
				((char *) arena->freelist + size);
			if((char *) arena + ARENA_SIZE
				- (char *) arena->freelist < size) {
				arena->freelist
					= ((struct free_block *) p)->next;
				arena->flags &= ~ARENA_NEW;
			}
		} else arena->freelist = arena->freelist->next;
		return p;
	}

	// Nope, we need a new arena
	arena = aligned_alloc(ARENA_SIZE,ARENA_SIZE);
	arena->next = *arenas;
	arena->size = size;
	arena->flags = ARENA_NEW;

	nblocks = (ARENA_SIZE - offsetof(arena_t,data))/(size + (float) 2/8);
	blocksoff = (offsetof(arena_t,data) + (nblocks*2 + 7)/8
		+ alignof(max_align_t) - 1)&~(alignof(max_align_t) - 1);

	debug("new arena:"
	    "\n\tbase address: %p"
	    "\n\ttotal size:   %i"
	    "\n\tchunk size:   %i"
	    "\n\tchunk count:  %i"
	    "\n\toverhead:     %i (%.2f%%)",
		arena,(int) ARENA_SIZE,(int) size,(int) nblocks,
		(int) blocksoff,100.*(ARENA_SIZE - nblocks*size)/ARENA_SIZE);

	((struct free_block *) ((char *) arena + blocksoff
		+ (nblocks - 1)*size))->next = NULL;

	arena->freelist
		= (struct free_block *) ((char *) arena + blocksoff + size);

	*arenas = arena;

	return (char *) arena + blocksoff;
}

void *mem_alloc(size_t size) {
	static struct {
		const size_t size;
		arena_t *arenas;
	} arenas[] = {
		{16,NULL},
		{32,NULL},
		{0,NULL} // Small < size < ARENA_MAX_ALLOC
	};

	// Large objects are handled separately
	if(size > ARENA_MAX_ALLOC)
		return malloc(size);

	// As are small objects
	for(int i = 0; arenas[i].size; i++)
		if(size <= arenas[i].size)
			return small_alloc(&arenas[i].arenas,arenas[i].size);

	error("unable to handle allocation of size %i",size);
	return malloc(size);
}

void mem_free(void *p) {
}

