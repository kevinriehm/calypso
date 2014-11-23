#ifndef STACK_H
#define STACK_H

#define STACK_ENSURE_SPACE(s, nbytes) ( \
	(s).top + (nbytes) - (s).bottom > (s).size \
		? grow_stack(&(s),(nbytes)) : (void) 0 \
)

#define STACK_ALLOC(s, type) ( \
	STACK_ENSURE_SPACE((s),sizeof(type)), \
	(s).top += sizeof(type), \
	STACK_TOP((s),type) \
)

#define STACK_FREE(s, type) ((s).top -= sizeof(type))

#define STACK_TOP(s, type) (*(type *) ((s).top - sizeof(type)))

#define STACK_PUSH(s, var) do { \
	STACK_ENSURE_SPACE(s, sizeof (var)); \
	memcpy((s).top,(void *) &(var),sizeof (var)); \
	(s).top += sizeof (var); \
} while(0)

#define STACK_POP(s, var) do { \
	assert((s).top - (s).bottom >= sizeof (var)); \
	(s).top -= sizeof (var); \
	memcpy((void *) &(var),(s).top,sizeof (var)); \
} while(0)

typedef struct stack {
	size_t size;
	char *bottom, *top;
} stack_t;

#endif

