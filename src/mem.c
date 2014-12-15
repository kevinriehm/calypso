#include <assert.h>
#include <math.h>
#include <stdalign.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "cell.h"
#include "env.h"
#include "htable.h"
#include "mem.h"
#include "repl.h"
#include "stack.h"
#include "util.h"
#include "va_macro.h"

#define BUDDY_MIN_EXP 6
#define BUDDY_MAX_EXP 20

#define BUDDY_MIN_ALLOC (1 << BUDDY_MIN_EXP)
#define BUDDY_MAX_ALLOC (1 << BUDDY_MAX_EXP - 1)

#define BUDDY_META_BYTES 1
#define BUDDY_SIZE_MASK  0x0f
#define BUDDY_ALLOC_BIT  0x10
#define BUDDY_GC_MASK    (BUDDY_GC1 | BUDDY_GC2)
#define BUDDY_GC1        0x40
#define BUDDY_GC2        0x80

static_assert((BUDDY_MAX_EXP - BUDDY_MIN_EXP&BUDDY_SIZE_MASK)
	== BUDDY_MAX_EXP - BUDDY_MIN_EXP,"BUDDY_SIZE_MASK too narrow");

#define ARENA_SIZE      (1 << BUDDY_MAX_EXP)

#define ARENA_TYPE_MASK 0x00000003ul
#define ARENA_FIXED     0x00000000ul
#define ARENA_BUDDY     0x00000001ul
#define ARENA_LARGE     0x00000002ul

#define ARENA_NEW       0x00000004ul

// For large-alloc arenas
#define ARENA_GC_MASK   (ARENA_GC1 | ARENA_GC2)
#define ARENA_GC1       0x00000008ul
#define ARENA_GC2       0x00000010ul

#define GC_USE_GROWTH 1.5

#define BUDDY_FLAGSP(arena, p) \
	((uint8_t *) ((arena)->data + BUDDY_META_BYTES \
		*((char *) (p) - (arena)->blocks)/BUDDY_MIN_ALLOC))

typedef struct free_block {
	struct free_block *next;
} free_block_t;

typedef struct bi_free_block {
	struct bi_free_block *next, *prev;
} bi_free_block_t;

static_assert(sizeof(bi_free_block_t) <= 1 << BUDDY_MIN_EXP,
	"BUDDY_MIN_EXP too small for bi_free_block_t");

typedef struct arena {
	struct arena *next;

	size_t size;
	uint32_t flags;
	char *blocks;

	size_t allocd;
	free_block_t *freelist;

	char data[];
} arena_t;

typedef void (*mark_func_t)(void *);

static struct {
	const size_t size;
	arena_t *arenas;
} fixedarenas[] = {
	{8,NULL},
	{16,NULL},
	{32,NULL},
	{0,NULL}
};

static struct {
	gc_type_t type;
	void *p;
} *handles; // For non-stack allocations
static int maxhandles, nhandles;

static arena_t *buddyarenas;
static bi_free_block_t buddyfree[BUDDY_MAX_EXP];

static arena_t *largearenas;

static int64_t heapsize = 0;      // Total of all arenas
static int64_t heapallocd = 0;    // Total of all allocs - frees
static int64_t heapused = 100000; // Updated each mem_gc()

static mark_func_t markfuncs[];

static bool gccolor = true; // Value of an unmarked object

static arena_t *alloc_arena() {
	heapsize += ARENA_SIZE;
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

	// Search the existing fixed-sized arenas first
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

		heapallocd += arena->size;
		arena->allocd += arena->size;
		return p;
	}

	// Nope, we need a new arena
	arena = alloc_arena();
	arena->next = *arenas;
	arena->size = size;
	arena->flags = ARENA_FIXED | ARENA_NEW;
	arena->allocd = 0;

	nblocks = (ARENA_SIZE - offsetof(arena_t,data))/(size + (float) 2/8);
	blocksoff = (offsetof(arena_t,data) + (nblocks*2 + 7)/8 + align - 1)
		&~(align - 1);

	arena->blocks = (char *) arena + blocksoff;

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

	arena->freelist = (free_block_t *) (arena->blocks + size);

	*arenas = arena;

	heapallocd += arena->size;
	arena->allocd = arena->size;

	return arena->blocks;
}

static void buddy_add_free_block(arena_t *arena, void *block, int sizeexp) {
	bi_free_block_t *_block = block;

	if(_block->next = buddyfree[sizeexp].next)
		_block->next->prev = _block;

	_block->prev = buddyfree + sizeexp;
	buddyfree[sizeexp].next = _block;

	arena->allocd -= 1 << sizeexp;
	heapallocd -= 1 << sizeexp;
}

static void buddy_claim_free_block(arena_t *arena, void *block, int sizeexp) {
	bi_free_block_t *_block = block;

	if(_block->prev->next = _block->next)
		_block->next->prev = _block->prev;

	arena->allocd += 1 << sizeexp;
	heapallocd += 1 << sizeexp;
}

static void buddy_split_block(arena_t *arena, void *block, int sizeexp) {
	bi_free_block_t *right;

	assert(sizeexp > BUDDY_MIN_EXP);

	right = (bi_free_block_t *) ((char *) block + (1 << sizeexp - 1));

	// Add right to the free list
	buddy_add_free_block(arena,right,sizeexp - 1);

	// Set right's flags
	*BUDDY_FLAGSP(arena,right) = sizeexp - 1 - BUDDY_MIN_EXP;
}

static void *buddy_check_free_lists(int sizeexp) {
	arena_t *arena;
	bi_free_block_t *block;

	for(int i = sizeexp; i < BUDDY_MAX_EXP; i++) {
		// Do we have a free one?
		if(block = buddyfree[i].next) {
			// In what arena?
			arena = (arena_t *)
				((uintptr_t) block&~(ARENA_SIZE - 1));

			// Claim it
			buddy_claim_free_block(arena,block,i);

			// Split it, as needed
			while(i > sizeexp)
				buddy_split_block(arena,block,i--);

			// Mark it as taken
			*BUDDY_FLAGSP(arena,block) = gccolor*BUDDY_GC1
				| BUDDY_ALLOC_BIT | i - BUDDY_MIN_EXP;

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
	nblocks = (ARENA_SIZE - headsize)/BUDDY_MIN_ALLOC;

	arena->blocks = (char *) arena + headsize;
	arena->allocd = ARENA_SIZE - (arena->blocks - (char *) arena);
	heapallocd += arena->allocd;

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

static void *large_alloc(size_t size) {
	size_t align;
	arena_t *arena;

	align = alignof(max_align_t);
	size += (offsetof(arena_t,data) + align - 1)&~(align - 1);
	if(posix_memalign((void **) &arena,ARENA_SIZE,size))
		die("cannot allocate %lli bytes",(long long) size);
	arena->flags = ARENA_LARGE;
	return (void *) ((uintptr_t) (arena->data + align - 1)&~(align - 1));
}

static void *large_free(arena_t *arena, void *p) {
	free(arena);
}

void *mem_alloc(size_t size) {
	int i;

	// Small objects have their own arenas
	for(i = 0; fixedarenas[i].size; i++)
		if(size <= fixedarenas[i].size)
			return fixed_alloc(&fixedarenas[i].arenas,
				fixedarenas[i].size);

	// Medium objects use the buddy system
	if(size < BUDDY_MAX_ALLOC)
		return buddy_alloc(&buddyarenas,size);

	// Large objects get their own arenas
	return large_alloc(size);
}

void mem_free(void *p) {
	if(!p) return;

	arena_t *arena;

	arena = (arena_t *) ((uintptr_t) p&~(ARENA_SIZE - 1));
	switch(arena->flags&ARENA_TYPE_MASK) {
	case ARENA_FIXED: fixed_free(arena,p); break;
	case ARENA_BUDDY: buddy_free(arena,p); break;
	case ARENA_LARGE: large_free(arena,p); break;

	default: die("unhandled arena type in mem_free(): 0x%08lu",
		arena->flags&ARENA_TYPE_MASK);
	}
}

void *mem_dup(void *p, size_t n) {
	return memcpy(mem_alloc(n),p,n);
}

#define QUAL_v
#define QUAL_pv   *
#define QUAL_vpv  *
#define QUAL_pvpv **

#define SQUAL_v
#define SQUAL_pv   p
#define SQUAL_vpv  p
#define SQUAL_pvpv pp

#define MARK_VAR_FROM_DATA(all, var) do { \
	memcpy(&evalvars.var,data,sizeof evalvars.var); \
	data += sizeof evalvars.var; \
	mark_##var(evalvars.var); \
} while(0)

#define HANDLE_STACK_FRAME(all, fcn) \
case PREFIX_BUILTIN(,fcn): \
	DEFER(EACH_INDIRECT)()(MARK_VAR_FROM_DATA,(;),,PRESERVE_##fcn); \
	break

#define MARK_TYPE(t, sq) MARK_TYPE_(t, sq)
#define MARK_TYPE_(type, squal) mark_##type##_##squal

#define MARK_SHIM(all, var) MARK_SHIM_(all, var)
#define MARK_SHIM_(t, q, v) MARK_SHIM__(t, q, SQUAL_##q, v)
#define MARK_SHIM__(t, q, sq, v) MARK_SHIM___(t, q, sq, v)
#define MARK_SHIM___(type, qual, squal, var) \
static inline void mark_##var(type QUAL_##qual x) { \
	MARK_TYPE(type,squal)(x); \
}

#define MARK_SHIMS(all, def) MARK_SHIMS_ def
#define MARK_SHIMS_(type, qual, vars) \
	DEFER(EACH_INDIRECT)()(MARK_SHIM,(),(type, qual),LITERAL vars)

static void MARK_TYPE(bool,       )(bool x)        {}
static void MARK_TYPE(bool,      p)(bool *x)       {}
static void MARK_TYPE(double,     )(double x)      {}
static void MARK_TYPE(int64_t,    )(int64_t x)     {}
static void MARK_TYPE(cell_type_t,)(cell_type_t x) {}

static void MARK_TYPE(cell_t,p)(cell_t *x) {
}

static void MARK_TYPE(cell_t,pp)(cell_t **x) {
	MARK_TYPE(cell_t,p)(*x);
}

static void MARK_TYPE(env_t,p)(env_t *x) {
}

static void MARK_TYPE(lambda_t,p)(lambda_t *x) {
}

EXPAND(EACH(MARK_SHIMS,(),(),EVAL_VARS));

// Mark-and-sweep
void mem_gc(stack_t *stack) {
	struct {
		EXPAND(EACH(PRINT_VARS,(;),(),EVAL_VARS));
	} evalvars;

	char *data;
	arena_t **arena;
	enum builtin type;
	int64_t oldheapsize, oldheapallocd;

	// Only do this if we need to
	if(heapallocd < GC_USE_GROWTH*heapused)
		return;

	debug("garbage collection (pre-cycle):"
	    "\n\theap size: %lli"
	    "\n\tallocated: %lli",
		(long long) heapsize,(long long) heapallocd);
	oldheapsize = heapsize;
	oldheapallocd = heapallocd;

	// Invert the meaning of all the GC bits
	gccolor = !gccolor;

	// Mark from the stack's root set
	data = stack->bottom;
	while(data < stack->top) {
		type = *(enum builtin *) data;
		data += sizeof type;

		// Handle the stack frame variables
		switch(type) {
			EXPAND(EACH(HANDLE_STACK_FRAME,(;),(),BUILTINS));
		}

		// Skip the jmp_buf
		data += sizeof(jmp_buf);
	}

	// Mark from the handles' root set
	for(int i = 0; i < nhandles; i++)
		markfuncs[handles[i].type](handles[i].p);
}

uint32_t mem_new_handle(gc_type_t type) {
	assert(type != GC_TYPE(etc));

	if(nhandles >= maxhandles) {
		maxhandles = 1.5*(maxhandles + 1);
		handles = realloc(handles,maxhandles*sizeof *handles);
		assert(handles);
	}

	handles[nhandles].type = type;
	handles[nhandles].p = NULL;

	return nhandles++;
}

void *mem_set_handle(uint32_t handle, void *p) {
	assert(handle < nhandles);

	handles[handle].p = p;
}

