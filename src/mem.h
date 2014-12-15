#ifndef MEM_H
#define MEM_H

#include <stddef.h>
#include <stdint.h>

#include "va_macro.h"

#define GC_TYPE(type) GC_TYPE_##type
#define GC_TYPE2(all, type) GC_TYPE(type)

#define GC_TYPES cell_t, env_t, hentry_t, htable_t, lambda_t, void

typedef enum gc_type {
	EACH(GC_TYPE2,(,),(),GC_TYPES),
	GC_TYPE(etc)
} gc_type_t;

struct stack;

void *mem_alloc(size_t);
void mem_free(void *);

void *mem_dup(void *, size_t);

void mem_gc(struct stack *);

#endif

