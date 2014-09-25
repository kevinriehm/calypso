#ifndef CELL_H
#define CELL_H

#include <stdint.h>

struct env;

typedef union cell_car {
	struct cell *p;

	enum cell_type {
		VAL_NIL,
		VAL_SYM,
		VAL_I64,
		VAL_DBL,
		VAL_CHR,
		VAL_STR,
		VAL_FCN,

		NUM_VAL_TYPES
	} type;
} cell_car_t;

typedef union cell_cdr {
	struct cell *p;

	int64_t i64;
	double dbl;

	char chr;
	char *str;

	struct cell *(*func)(struct env *, struct cell *);
} cell_cdr_t;

typedef struct cell {
	union cell_car car;
	union cell_cdr cdr;
} cell_t;

cell_t *cell_cons(cell_t *, cell_t *);
cell_t *cell_cons_t(enum cell_type, ...);

#endif

