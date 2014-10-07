#ifndef CELL_H
#define CELL_H

#include <stdbool.h>
#include <stdint.h>

struct env;

typedef struct lambda {
	bool ismacro;
	struct env *env;
	struct cell *args;
	struct cell *body;
} lambda_t;

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
		VAL_LBA,

		NUM_VAL_TYPES
	} type;
} cell_car_t;

typedef union cell_cdr {
	struct cell *p;

	int64_t i64;
	double dbl;

	char chr;
	char *str;

	struct cell *(*fcn)(struct env *, struct cell *);

	struct lambda *lba;
} cell_cdr_t;

typedef struct cell {
	union cell_car car;
	union cell_cdr cdr;
} cell_t;

cell_t *cell_cons(cell_t *, cell_t *);
cell_t *cell_cons_t(enum cell_type, ...);
cell_t *cell_dup(cell_t *);

bool cell_is_atom(cell_t *);
bool cell_is_list(cell_t *);

#endif

