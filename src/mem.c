#include <assert.h>
#include <stdalign.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

#include "util.h"

#define BUDDY_MIN_EXP    6
#define BUDDY_MAX_EXP    20
#define BUDDY_NUM_LEVELS (BUDDY_MAX_EXP - BUDDY_MIN_EXP) // Sic

#define BUDDY_MIN_ALLOC (1 << BUDDY_MIN_EXP)

#define BUDDY_META_BYTES 1
#define BUDDY_LEVEL_MASK 0x0f
#define BUDDY_FREE_MASK  0x10
#define BUDDY_GC_MASK    0xc0

#define ARENA_SIZE      (1 << BUDDY_MAX_EXP)
#define ARENA_MAX_ALLOC (1 << BUDDY_MAX_EXP - 1)

#define ARENA_TYPE_MASK 0x00000003ul
#define ARENA_FIXED     0x00000000ul
#define ARENA_BUDDY     0x00000001ul
#define ARENA_LARGE     0x00000002ul

#define ARENA_NEW       0x00000004ul

typedef struct free_block {
	struct free_block *next;
} free_block_t;

typedef struct bi_free_block {
	struct bi_free_block *next, *prev;
} bi_free_block_t;

static_assert(sizeof(bi_free_block_t) <= 1 << BUDDY_MIN_EXP,
	"BUDDY_MIN_EXP too small");

typedef struct arena {
	struct arena *next;

	size_t size;
	uint32_t flags;

	free_block_t *freelist;

	uint8_t data[];
} arena_t;

static arena_t *alloc_arena() {
	return aligned_alloc(ARENA_SIZE,ARENA_SIZE);
}

static void *fixed_alloc(arena_t **arenas, size_t size) {
	void *p;
	arena_t *arena;
	size_t align, blocksoff, nblocks;

	assert(size != 0 && !(size & size - 1));
	assert(size >= sizeof(free_block_t));

	if(size < alignof(max_align_t))
		align = size;
	else align = alignof(max_align_t);

	// Search the existing arenas first
	for(arena = *arenas; arena && !arena->freelist; arena = arena->next);

	// Did we find one?
	if(arena) {
		p = arena->freelist;
		if(arena->flags & ARENA_NEW) {
			arena->freelist = (free_block_t *) 
				((char *) arena->freelist + size);
			if((char *) arena + ARENA_SIZE
				- (char *) arena->freelist < size) {
				arena->freelist = ((free_block_t *) p)->next;
				arena->flags &= ~ARENA_NEW;
			}
		} else arena->freelist = arena->freelist->next;
		return p;
	}

	// Nope, we need a new arena
	arena = alloc_arena();
	arena->next = *arenas;
	arena->size = size;
	arena->flags = ARENA_FIXED | ARENA_NEW;

	nblocks = (ARENA_SIZE - offsetof(arena_t,data))/(size + (float) 2/8);
	blocksoff = (offsetof(arena_t,data) + (nblocks*2 + 7)/8 + align - 1)
		&~(align - 1);

	debug("new fixed-size allocation arena:"
	    "\n\tbase address: %p"
	    "\n\ttotal size:   %i"
	    "\n\tchunk size:   %i"
	    "\n\tchunk count:  %i"
	    "\n\toverhead:     %i (%.2f%%)",
		arena,(int) ARENA_SIZE,(int) size,(int) nblocks,
		(int) (ARENA_SIZE - nblocks*size),
		100.*(ARENA_SIZE - nblocks*size)/ARENA_SIZE);

	((free_block_t *) ((char *) arena + blocksoff + (nblocks - 1)*size))
		->next = NULL;

	arena->freelist = (free_block_t *) ((char *) arena + blocksoff + size);

	*arenas = arena;

	return (char *) arena + blocksoff;
}

// Binary buddy allocation
static void *buddy_alloc(arena_t **arenas, size_t size) {
	static bi_free_block_t freelists[BUDDY_NUM_LEVELS];

	int sizei;
	arena_t *arena;
	size_t headsize, nblocks;
	bi_free_block_t *block, *half;

	// Find a suitable size
	static_assert(SIZE_MAX <= UINT64_MAX,"size_t is more than 64 bits");
	sizei = 0;
	size = (size - 1)*2;
	if(size >= 1ll << 32) sizei += 32, size >>= 32;
	if(size >= 1l  << 16) sizei += 16, size >>= 16;
	if(size >= 1   <<  8) sizei +=  8, size >>=  8;
	if(size >= 1   <<  4) sizei +=  4, size >>=  4;
	if(size >= 1   <<  2) sizei +=  2, size >>=  2;
	if(size >= 1   <<  1) sizei +=  1, size >>=  1;
	sizei = sizei < BUDDY_MIN_EXP ? 0 : sizei - BUDDY_MIN_EXP;
	assert(sizei < BUDDY_NUM_LEVELS);

	// Check the freelists
	arena = NULL;
check_freelists:
	for(int i = sizei; i < BUDDY_NUM_LEVELS; i++) {
		// Do we have a free one?
		if(block = freelists[i].next) {
			// Claim it
			freelists[i].next = block->next;
			if(freelists[i].next)
				freelists[i].next->prev = freelists  + i;

			// Split it, as needed
			while(i > sizei) {
				i--;
				half = (bi_free_block_t *)
					((char *) block
					+ (1 << BUDDY_MIN_EXP + i)); 
				// TODO: set half's flags
				half->next = freelists[i].next;
				half->prev = freelists + i;
				if(half->next)
					half->next->prev = half;
				freelists[i].next = half;
			}

			return block;
		}
	}

	// Sanity check
	assert(!arena);

	// We need a new arena
	arena = alloc_arena();
	arena->next = *arenas;
	arena->flags = ARENA_BUDDY;

	nblocks = (ARENA_SIZE - offsetof(arena_t,data))
		/(BUDDY_MIN_ALLOC + BUDDY_META_BYTES);
	headsize = (offsetof(arena_t,data) + nblocks*BUDDY_META_BYTES
		+ BUDDY_MIN_ALLOC - 1)&~((size_t) BUDDY_MIN_ALLOC - 1);

	debug("new buddy-allocation arena:"
	    "\n\tbase address:    %p"
	    "\n\ttotal size:      %i"
	    "\n\tmin chunk size:  %i"
	    "\n\tmin chunk count: %i"
	    "\n\toverhead:        %i (%.2f%%)",
		arena,(int) ARENA_SIZE,(int) 1 << BUDDY_MIN_EXP,(int) nblocks,
		(int) (ARENA_SIZE - nblocks*BUDDY_MIN_ALLOC),
		100.*(ARENA_SIZE - nblocks*BUDDY_MIN_ALLOC)/ARENA_SIZE);

	// Split up the new arena for the header
	for(int i = BUDDY_NUM_LEVELS - 1;
		i >= 0 && 1 << (BUDDY_MIN_EXP + i) >= headsize; i--) {
		half = (bi_free_block_t *)
			((char *) arena + (1 << BUDDY_MIN_EXP + i));
		// TODO: set half's flags
		half->next = freelists[i].next;
		half->prev = freelists + i;
		if(half->next)
			half->next->prev = half;
		freelists[i].next = half;
	}

	*arenas = arena;

	// Now we definitely have room
	goto check_freelists;
}

void *mem_alloc(size_t size) {
	static struct {
		const size_t size;
		arena_t *arenas;
	} arenas[] = {
		{8,NULL},
		{16,NULL},
		{32,NULL},
		{0,NULL} // Small < size < ARENA_MAX_ALLOC
	};

	int i;
	size_t align;
	arena_t *arena;

	// Small objects have their own arenas
	for(i = 0; arenas[i].size; i++)
		if(size <= arenas[i].size)
			return fixed_alloc(&arenas[i].arenas,arenas[i].size);

	// Medium objects use the buddy system
	if(size < ARENA_MAX_ALLOC)
		return buddy_alloc(&arenas[i].arenas,size);

	// Large objects get their own arenas
	align = alignof(max_align_t);
	size += (offsetof(arena_t,data) + align - 1)&~(align - 1);
	if(posix_memalign((void **) &arena,ARENA_SIZE,size))
		die("cannot allocate %lli bytes",(long long) size);
	arena->flags = ARENA_LARGE;
	return (void *) ((uintptr_t) (arena->data + align - 1)&~(align - 1));
}

void mem_free(void *p) {
}

