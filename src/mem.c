#include <assert.h>
#include <math.h>
#include <stdalign.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

#include "util.h"

#define BUDDY_MIN_EXP 6
#define BUDDY_MAX_EXP 20

#define BUDDY_MIN_ALLOC (1 << BUDDY_MIN_EXP)
#define BUDDY_MAX_ALLOC (1 << BUDDY_MAX_EXP - 1)

#define BUDDY_META_BYTES 1
#define BUDDY_SIZE_MASK  0x0f
#define BUDDY_ALLOC_BIT  0x10
#define BUDDY_GC_MASK    0xc0

static_assert((BUDDY_MAX_EXP - BUDDY_MIN_EXP&BUDDY_SIZE_MASK)
	== BUDDY_MAX_EXP - BUDDY_MIN_EXP,"BUDDY_SIZE_MASK too narrow");

#define ARENA_SIZE      (1 << BUDDY_MAX_EXP)

#define ARENA_TYPE_MASK 0x00000003ul
#define ARENA_FIXED     0x00000000ul
#define ARENA_BUDDY     0x00000001ul
#define ARENA_LARGE     0x00000002ul

#define ARENA_NEW       0x00000004ul

#define BUDDY_FLAGSP(arena, p) ((uint8_t *) \
	(arena->data + ((char *) (p) - arena->blocks)/BUDDY_MIN_ALLOC))

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

	union {
		free_block_t *freelist;
		char *blocks;
	};

	char data[];
} arena_t;

static bi_free_block_t buddyfree[BUDDY_MAX_EXP];

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

static void fixed_free(arena_t *arena, void *p) {
	assert((arena->flags&ARENA_TYPE_MASK) == ARENA_FIXED);

	((free_block_t *) p)->next = arena->freelist;
	arena->freelist = p;
}

static void buddy_split_block(arena_t *arena, void *block, int sizeexp) {
	uint8_t *flags;
	bi_free_block_t *right;

	right = (bi_free_block_t *) ((char *) block + (1 << sizeexp - 1));

	// Add right to the free list
	if(right->next = buddyfree[sizeexp - 1].next)
		right->next->prev = right;
	right->prev = buddyfree + sizeexp - 1;
	buddyfree[sizeexp - 1].next = right;

	// Set right's flags
	flags = BUDDY_FLAGSP(arena,right);
	*flags = *flags&BUDDY_GC_MASK | sizeexp - 1 - BUDDY_MIN_EXP;
}

static void *buddy_check_free_lists(int sizeexp) {
	arena_t *arena;
	uint8_t *flags;
	bi_free_block_t *block;

	for(int i = sizeexp; i < BUDDY_MAX_EXP; i++) {
		// Do we have a free one?
		if(block = buddyfree[i].next) {
			// Claim it
			if(buddyfree[i].next = block->next)
				buddyfree[i].next->prev = buddyfree  + i;

			// In what arena?
			arena = (arena_t *)
				((uintptr_t) block&~(ARENA_SIZE - 1));

			// Split it, as needed
			while(i > sizeexp)
				buddy_split_block(arena,block,i--);

			// Mark it as taken
			flags = BUDDY_FLAGSP(arena,block);
			*flags = *flags&BUDDY_GC_MASK | BUDDY_ALLOC_BIT
				| i - BUDDY_MIN_EXP;

			return block;
		}
	}

	return NULL;
}

// Binary buddy allocation
static void *buddy_alloc(arena_t **arenas, size_t size) {
	int sizei;
	arena_t *arena;
	size_t headsize, nblocks;
	bi_free_block_t *block, *half;

	// Find a suitable size
	assert(size < 1 << BUDDY_MAX_EXP);
	sizei = 0;
	size = (size - 1)*2;
	if(size >= 1l << 16) sizei += 16, size >>= 16;
	if(size >= 1  <<  8) sizei +=  8, size >>=  8;
	if(size >= 1  <<  4) sizei +=  4, size >>=  4;
	if(size >= 1  <<  2) sizei +=  2, size >>=  2;
	if(size >= 1  <<  1) sizei +=  1, size >>=  1;
	assert(sizei < BUDDY_MAX_EXP);

	// Check the free lists
	if(block = buddy_check_free_lists(sizei))
		return block;

	// We need a new arena
	arena = alloc_arena();
	arena->next = *arenas;
	arena->flags = ARENA_BUDDY;

	nblocks = (ARENA_SIZE - offsetof(arena_t,data))
		/(BUDDY_MIN_ALLOC + BUDDY_META_BYTES);
	headsize = offsetof(arena_t,data) + nblocks*BUDDY_META_BYTES;
	headsize = 1 << (int) ceil(log2(headsize));

	arena->blocks = (char *) arena + headsize;

	debug("new buddy-allocation arena:"
	    "\n\tbase address:    %p"
	    "\n\ttotal size:      %i"
	    "\n\tmin chunk size:  %i"
	    "\n\tmin chunk count: %i"
	    "\n\toverhead:        %i (%.2f%%)",
		arena,(int) ARENA_SIZE,(int) 1 << BUDDY_MIN_EXP,(int) nblocks,
		(int) headsize,100.*headsize/ARENA_SIZE);

	// Split up the new arena for the header
	for(int i = BUDDY_MAX_EXP - 1; 1 << i >= headsize; i--)
		buddy_split_block(arena,arena,i + 1);

	*arenas = arena;

	// Now we definitely have room
	return buddy_check_free_lists(sizei);
}

static void buddy_free(arena_t *arena, void *p) {
	assert((arena->flags&ARENA_TYPE_MASK) == ARENA_BUDDY);

	int sizeexp;
	void *buddy;
	uint8_t *bflags, *pflags;

	pflags = BUDDY_FLAGSP(arena,p);
	sizeexp = BUDDY_MIN_EXP + (*pflags&BUDDY_SIZE_MASK);

	while(true) {
		buddy = (void *) ((uintptr_t) p ^ 1 << sizeexp);
		bflags = BUDDY_FLAGSP(arena,buddy);

		if(*bflags&BUDDY_ALLOC_BIT)
			break;

		sizeexp++;

		if(buddy < p) {
			p = buddy;
			pflags = bflags;
		}
	}

	*pflags = (*pflags&BUDDY_GC_MASK) | sizeexp - BUDDY_MIN_EXP;
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
	if(size < BUDDY_MAX_ALLOC)
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
	if(!p) return;

	arena_t *arena;

	arena = (arena_t *) ((uintptr_t) p&~(ARENA_SIZE - 1));
	switch(arena->flags&ARENA_TYPE_MASK) {
	case ARENA_FIXED: fixed_free(arena,p); break;
	case ARENA_BUDDY: buddy_free(arena,p); break;
	case ARENA_LARGE: free(arena); break;

	default: die("unhandled arena type in mem_free(): 0x%08lu",
		arena->flags&ARENA_TYPE_MASK);
	}
}

