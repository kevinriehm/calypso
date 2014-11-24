#ifndef MEM_H
#define MEM_H

#include <stddef.h>

struct stack;

void *mem_alloc(size_t);
void mem_free(void *);

void *mem_dup(void *, size_t);

void mem_gc(struct stack *);

#endif

