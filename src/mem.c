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

#define GC_NUM_BITS   2
#define GC_USE_GROWTH 5

#define GC_COLOR(bit0, bit1, flags) ((flags) & ((bit0) | (bit1)))
#define GC_FREE                     0
#define GC_WHITE(bit0, bit1)        (gcinvert ? (bit0) : (bit1))
#define GC_BLACK(bit0, bit1)        (gcinvert ? (bit1) : (bit0))

#define FIXED_GC_MASK(i) (3 << (i))

#define FIXED_GC_COLOR(flags, i) GC_COLOR(1 << (i),2 << (i),(flags))
#define FIXED_GC_FREE            GC_FREE
#define FIXED_GC_WHITE(i)        GC_WHITE(1 << (i),2 << (i))
#define FIXED_GC_BLACK(i)        GC_BLACK(1 << (i),2 << (i))

#define BUDDY_MIN_EXP 6
#define BUDDY_MAX_EXP 20

#define BUDDY_MIN_ALLOC (1 << BUDDY_MIN_EXP)
#define BUDDY_MAX_ALLOC (1 << BUDDY_MAX_EXP - 1)

#define BUDDY_META_BYTES 1
#define BUDDY_SIZE_MASK  0x0f
#define BUDDY_GC_MASK    (BUDDY_GC1 | BUDDY_GC2)
#define BUDDY_GC1        0x40
#define BUDDY_GC2        0x80

#define BUDDY_GC_COLOR(flags) GC_COLOR(BUDDY_GC1,BUDDY_GC2,(flags))
#define BUDDY_GC_FREE         GC_FREE
#define BUDDY_GC_WHITE        GC_WHITE(BUDDY_GC1,BUDDY_GC2)
#define BUDDY_GC_BLACK        GC_BLACK(BUDDY_GC1,BUDDY_GC2)

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

#define ARENA_GC_COLOR(flags) GC_COLOR(ARENA_GC1,ARENA_GC2,(flags))
#define ARENA_GC_FREE         GC_FREE
#define ARENA_GC_WHITE        GC_WHITE(ARENA_GC1,ARENA_GC2)
#define ARENA_GC_BLACK        GC_BLACK(ARENA_GC1,ARENA_GC2)

#define BUDDY_FLAGSP(arena, p) \
	((uint8_t *) ((arena)->data + BUDDY_META_BYTES \
		*((char *) (p) - (arena)->blocks)/BUDDY_MIN_ALLOC))

typedef struct free_block {
	struct free_block *next;
} free_block_t;

typedef struct bi_free_block {
	struct bi_free_block *next, *prev;
} bi_free_block_t;

static_assert(sizeof(bi_free_block_t) <= BUDDY_MIN_ALLOC,
	"BUDDY_MIN_EXP too small for bi_free_block_t");

typedef struct arena {
	struct arena *next;

	size_t size;
	uint32_t flags;
	char *blocks;

	free_block_t *freelist;

	char data[];
} arena_t;

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
static uint32_t maxhandles, nhandles;

static arena_t *buddyarenas;
static bi_free_block_t buddyfree[BUDDY_MAX_EXP];

static arena_t *largearenas;

static int64_t heapsize = 0;      // Total of all arenas
static int64_t heapallocd = 0;    // Total of all allocs - frees
static int64_t heapused = 100000; // Updated each mem_gc()

static bool gcinvert = true; // Swaps the meaning of white and black GC bits

static arena_t *alloc_arena() {
	heapsize += ARENA_SIZE;
	return aligned_alloc(ARENA_SIZE,ARENA_SIZE);
}

static void *fixed_alloc(arena_t **arenas, size_t size) {
	void *p;
	int gcbitsi;
	arena_t *arena;
	uint8_t *flagsp;
	size_t align, blocksoff, nblocks;

	assert(size != 0 && !(size & size - 1));
	assert(size >= sizeof(free_block_t));

	// Search the existing fixed-sized arenas first
	for(arena = *arenas; arena && !arena->freelist; arena = arena->next);

	// Did we find one?
	if(arena) {
		p = arena->freelist;

		// Set the flags
		gcbitsi = GC_NUM_BITS*((char *) p - arena->blocks)/arena->size;
		flagsp = (uint8_t *) arena->data + gcbitsi/8;
		*flagsp = *flagsp&~FIXED_GC_MASK(gcbitsi%8)
			| FIXED_GC_BLACK(gcbitsi%8);

		if(arena->flags&ARENA_NEW) {
			arena->freelist = (free_block_t *) 
				((char *) arena->freelist + size);
			if((char *) arena + ARENA_SIZE
				- (char *) arena->freelist
				< (ptrdiff_t) size) {
				arena->freelist = ((free_block_t *) p)->next;
				arena->flags &= ~ARENA_NEW;
			}
		} else arena->freelist = arena->freelist->next;

		heapallocd += arena->size;

		return p;
	}

	// Nope, we need a new arena
	arena = alloc_arena();
	arena->next = *arenas;
	arena->size = size;
	arena->flags = ARENA_FIXED | ARENA_NEW;

	if(size < alignof(max_align_t))
		align = size;
	else align = alignof(max_align_t);

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

	// Set the flags
	flagsp = (uint8_t *) arena->data;
	*flagsp = *flagsp&~FIXED_GC_MASK(0) | FIXED_GC_BLACK(0);

	heapallocd += arena->size;

	return arena->blocks;
}

static void buddy_add_free_block(void *block, int sizeexp) {
	bi_free_block_t *_block = block;

	if(_block->next = buddyfree[sizeexp].next)
		_block->next->prev = _block;

	_block->prev = buddyfree + sizeexp;
	buddyfree[sizeexp].next = _block;
}

static void buddy_claim_free_block(void *block) {
	bi_free_block_t *_block = block;

	if(_block->prev->next = _block->next)
		_block->next->prev = _block->prev;
}

static void buddy_split_block(arena_t *arena, void *block, int sizeexp) {
	bi_free_block_t *right;

	assert(sizeexp > BUDDY_MIN_EXP);

	right = (bi_free_block_t *) ((char *) block + (1 << sizeexp - 1));

	// right is free
	buddy_add_free_block(right,sizeexp - 1);

	// Set right's flags
	*BUDDY_FLAGSP(arena,right) = BUDDY_GC_FREE
		| sizeexp - 1 - BUDDY_MIN_EXP;
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
			buddy_claim_free_block(block);

			// Split it, as needed
			while(i > sizeexp)
				buddy_split_block(arena,block,i--);

			// Mark it as taken
			*BUDDY_FLAGSP(arena,block) = BUDDY_GC_BLACK
				| i - BUDDY_MIN_EXP;

			heapallocd += 1 << sizeexp;

			return block;
		}
	}

	return NULL;
}

// Binary buddy allocation
static void *buddy_alloc(arena_t **arenas, size_t size) {
	int sizei;
	arena_t *arena;
	bi_free_block_t *block;
	size_t headsize, nblocks;

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

	debug("new buddy-allocation arena:"
	    "\n\tbase address:    %p"
	    "\n\ttotal size:      %i"
	    "\n\tmin chunk size:  %i"
	    "\n\tmin chunk count: %i"
	    "\n\toverhead:        %i (%.2f%%)",
		arena,(int) ARENA_SIZE,(int) BUDDY_MIN_ALLOC,(int) nblocks,
		(int) headsize,100.*headsize/ARENA_SIZE);

	// Split up the new arena for the header
	for(int i = BUDDY_MAX_EXP - 1; (size_t) 1 << i >= headsize; i--)
		buddy_split_block(arena,arena,i + 1);

	*arenas = arena;

	// Now we definitely have room
	return buddy_check_free_lists(sizei);
}

static void *large_alloc(size_t size) {
	arena_t *arena;
	size_t align, headsize;

	align = alignof(max_align_t);
	headsize = (offsetof(arena_t,data) + align - 1)&~(align - 1);

	if(posix_memalign((void **) &arena,ARENA_SIZE,size + headsize))
		die("cannot allocate %lli bytes",(long long) size);
	arena->size = size;
	arena->flags = ARENA_LARGE | ARENA_GC_WHITE;
	arena->blocks = (char *) arena + headsize;

	heapsize += headsize + size;
	heapallocd += size;

	return (void *) ((uintptr_t) (arena->data + align - 1)&~(align - 1));
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

// These are assumed not to be allocated with mem_alloc()
static void MARK_TYPE(bool,       )(bool x)        { (void) x; }
static void MARK_TYPE(double,     )(double x)      { (void) x; }
static void MARK_TYPE(int64_t,    )(int64_t x)     { (void) x; }
static void MARK_TYPE(cell_type_t,)(cell_type_t x) { (void) x; }

// These are assumed to be allocated with mem_alloc()

// Forward declarations
#define DECLARE_MARK_GC_TYPE(all, type) \
static void MARK_TYPE(type,p)(type *)

EACH(DECLARE_MARK_GC_TYPE,(;),(),GC_TYPES);

#define DECLARE_MARK_GC_TYPE_INDIRECT(all, type) \
static void MARK_TYPE(type,pp)(type **x) { \
	if(x) MARK_TYPE(type,p)(*x); \
}

EACH(DECLARE_MARK_GC_TYPE_INDIRECT,(),(),GC_TYPES)

typedef void (*mark_func_t)(void *);

#define REGISTER_MARK_FUNC(all, type) \
	[GC_TYPE(type)] = (mark_func_t) MARK_TYPE(type,p), \
	[GC_TYPE_INDIRECT(type)] = (mark_func_t) MARK_TYPE(type,pp)

static const mark_func_t markfuncs[] = {
	EACH(REGISTER_MARK_FUNC,(,),(),GC_TYPES)
};

static void MARK_TYPE(lambda_t,)(lambda_t x) {
	MARK_TYPE(env_t,p)(x.env);
	MARK_TYPE(cell_t,p)(x.args);
	MARK_TYPE(cell_t,p)(x.body);
}

// Returns whether p was already marked
static bool mark_ptr(void *p) {
	int gcbitsi;
	bool marked;
	size_t size;
	arena_t *arena;
	uint8_t *flagsp;

	// Sanity check
	if(!p) return true;

	arena = (arena_t *) ((uintptr_t) p&~(ARENA_SIZE - 1));

	// What kind of arena?
	switch(arena->flags&ARENA_TYPE_MASK) {
	case ARENA_FIXED:
		// Fixed GC bits are all packed together
		gcbitsi = GC_NUM_BITS*((char *) p - arena->blocks)/arena->size;
		flagsp = (uint8_t *) arena->data + gcbitsi/8;
		marked = FIXED_GC_COLOR(*flagsp,gcbitsi%8)
			== FIXED_GC_BLACK(gcbitsi%8);
		*flagsp = *flagsp&~FIXED_GC_MASK(gcbitsi%8)
			| FIXED_GC_BLACK(gcbitsi%8);

		size = arena->size;
		break;

	case ARENA_BUDDY:
		// Each buddy block gets its own byte for flags
		flagsp = BUDDY_FLAGSP(arena,p);
		marked = BUDDY_GC_COLOR(*flagsp) == BUDDY_GC_BLACK;
		*flagsp = *flagsp&~BUDDY_GC_MASK | BUDDY_GC_BLACK;

		size = 1 << BUDDY_MIN_EXP + (*flagsp&BUDDY_SIZE_MASK);
		break;

	case ARENA_LARGE:
		// Large arenas are individual allocations
		marked = ARENA_GC_COLOR(arena->flags) == ARENA_GC_BLACK;
		arena->flags = arena->flags&ARENA_GC_MASK | ARENA_GC_BLACK;

		size = arena->size;
		break;

	default: die("unhandled arena type in mark_ptr(): 0x%08u",
		arena->flags&ARENA_TYPE_MASK); break;
	}

	if(!marked)
		heapallocd += size;

	return marked;
}

static void MARK_TYPE(string_t,p)(string_t *x) {
	mark_ptr(x);
}

static void MARK_TYPE(cell_t,p)(cell_t *x) {
	if(mark_ptr(x))
		return;

	switch(cell_type(x)) {
	case VAL_SYM:
		MARK_TYPE(string_t,p)(x->sym);
		break;

	case VAL_STR:
		MARK_TYPE(string_t,p)(x->str);
		break;

	case VAL_LBA:
		MARK_TYPE(lambda_t,)(*cell_lba(x));
		break;

	case VAL_LST:
		MARK_TYPE(cell_t,p)(x->car);
		MARK_TYPE(cell_t,p)(x->cdr);
		break;

	default: break;
	}
}

static void MARK_TYPE(env_t,p)(env_t *x) {
	if(mark_ptr(x))
		return;

	MARK_TYPE(env_t,p)(x->parent);
	MARK_TYPE(htable_t,p)(x->tab);
}

static void MARK_TYPE(hentry_t,p)(hentry_t *x) {
	for(; x; x = x->next) {
		if(mark_ptr(x))
			return;

		MARK_TYPE(void,p)(x->key);

		if(x->val.type != GC_TYPE(etc))
			markfuncs[x->val.type](x->val.p);
	}
}

static void MARK_TYPE(htable_t,p)(htable_t *x) {
	if(mark_ptr(x))
		return;

	mark_ptr(x->entries);

	for(uint32_t i = 0; i < x->cap; i++)
		MARK_TYPE(hentry_t,p)(x->entries[i]);
}

static void MARK_TYPE(lambda_t,p)(lambda_t *x) {
	if(mark_ptr(x))
		return;

	MARK_TYPE(lambda_t,)(*x);
}

static void MARK_TYPE(void,p)(void *p) {
	mark_ptr(p);
}

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

EXPAND(EACH(MARK_SHIMS,(),(),EVAL_VARS))

static void clean_fixed_arena(arena_t **arena) {
	char *flagsp;
	long gcbitsi, nblocks;
	free_block_t *freeblock, **freelist;

	// How many blocks to check, and where to put newly freed blocks
	if((*arena)->flags&ARENA_NEW) {
		nblocks = ((char *) (*arena)->freelist - (*arena)->blocks)
			/(*arena)->size;
		freelist = &((free_block_t *) ((uintptr_t) *arena + ARENA_SIZE
			- (*arena)->size))->next;
	} else {
		nblocks = ((char *) *arena + ARENA_SIZE - (*arena)->blocks)
			/(*arena)->size;
		freelist = &(*arena)->freelist;
	}

	// Check every block
	for(long i = 0; i < nblocks; i++) {
		gcbitsi = GC_NUM_BITS*i;

		flagsp = (*arena)->data + gcbitsi/8;

		if(FIXED_GC_COLOR(*flagsp,gcbitsi%8)
			== FIXED_GC_WHITE(gcbitsi%8)) {
			freeblock = (free_block_t *)
				((*arena)->blocks + i*(*arena)->size);
			freeblock->next = *freelist;
			*freelist = freeblock;

			*flagsp = *flagsp&~FIXED_GC_MASK(gcbitsi%8)
				| FIXED_GC_FREE;
		}
	}
}

static void clean_buddy_arena(arena_t **arena) {
	int sizeexp;
	char *buddy, *endp, *p;
	uint8_t *bflagsp, *flagsp;

	// Step through all the blocks
	endp = (char *) *arena + ARENA_SIZE;
	for(p = (*arena)->blocks; p < endp; p += 1 << sizeexp) {
		flagsp = BUDDY_FLAGSP(*arena,p);
		sizeexp = BUDDY_MIN_EXP + (*flagsp&BUDDY_SIZE_MASK);

		// Only free allocated but unmarked blocks
		if(BUDDY_GC_COLOR(*flagsp) != BUDDY_GC_WHITE)
			continue;

		// Merge as many buddies as possible
		while(true) {
			buddy = (char *) ((uintptr_t) p ^ 1 << sizeexp);

			// Invalid buddy?
			if(buddy < (*arena)->blocks)
				break;

			bflagsp = BUDDY_FLAGSP(*arena,buddy);

			// Buddy not mergable?
			if(BUDDY_GC_COLOR(*bflagsp) == BUDDY_GC_BLACK
				|| sizeexp != BUDDY_MIN_EXP
					+ (*bflagsp&BUDDY_SIZE_MASK))
				break;

			// Claim it for accounting purposes
			if(BUDDY_GC_COLOR(*bflagsp) == BUDDY_GC_FREE)
				buddy_claim_free_block(buddy);

			sizeexp++;

			if(buddy < p) {
				p = buddy;
				flagsp = bflagsp;
			}
		}

		// 2^sizeexp bytes at p are free
		buddy_add_free_block(p,sizeexp);

		*flagsp = BUDDY_GC_FREE | sizeexp - BUDDY_MIN_EXP;
	}
}

static void clean_large_arena(arena_t **arena) {
	arena_t *next;
	size_t headsize;

	// Leave it if marked
	if(ARENA_GC_COLOR((*arena)->flags) == ARENA_GC_BLACK)
		return;

	// Free the whole arena
	headsize = (*arena)->blocks - (char *) *arena;
	heapsize -= headsize + (*arena)->size;

	next = (*arena)->next;
	free(*arena);
	*arena = next;
}

// Mark-and-sweep
void mem_gc(stack_t *stack) {
	struct {
		EXPAND(EACH(PRINT_VARS,(;),(),EVAL_VARS));
	} evalvars;

	char *data;
	arena_t **arena;
	enum builtin type;
	int64_t oldheapsize, oldheapallocd;

	(void) oldheapsize;
	(void) oldheapallocd;

	// Only do this if we need to
	if(heapallocd < GC_USE_GROWTH*heapused)
		return;

	debug("garbage collection (pre-cycle):"
	    "\n\theap size: %lli"
	    "\n\tallocated: %lli",
		(long long) heapsize,(long long) heapallocd);
	oldheapsize = heapsize;
	oldheapallocd = heapallocd;

	// Accounting
	heapallocd = 0;

	// Invert the meaning of all the GC bits
	gcinvert = !gcinvert;

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
	for(uint32_t i = 0; i < nhandles; i++)
		markfuncs[handles[i].type](handles[i].p);

	// Clean out each of the arenas
	for(int i = 0; fixedarenas[i].arenas; i++)
		for(arena = &fixedarenas[i].arenas; *arena;
			*arena ? arena = &(*arena)->next : 0)
			clean_fixed_arena(arena);

	for(arena = &buddyarenas; *arena; *arena ? arena = &(*arena)->next : 0)
		clean_buddy_arena(arena);

	for(arena = &largearenas; *arena; *arena ? arena = &(*arena)->next : 0)
		clean_large_arena(arena);

	heapused = heapallocd;
	assert(heapused >= 0);

	debug("garbage collection (post-cycle):"
	    "\n\theap size: %lli (%+.2f%%)"
	    "\n\tallocated: %lli (%+.2f%%)",
		(long long) heapsize,
		100.*(heapsize - oldheapsize)/oldheapsize,
		(long long) heapallocd,
		100.*(heapallocd - oldheapallocd)/oldheapallocd);
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

	return p;
}

