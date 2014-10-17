#ifndef CELL_H
#define CELL_H

#include <stdbool.h>
#include <stdint.h>

struct env;

typedef enum cell_type {
	VAL_NIL,
	VAL_SYM,
	VAL_I64,
	VAL_DBL,
	VAL_CHR,
	VAL_STR,
	VAL_FCN,
	VAL_LBA,

	NUM_VAL_TYPES,

	VAL_LST
} cell_type_t;

typedef struct lambda {
	bool ismacro;
	struct env *env;
	struct cell *args;
	struct cell *body;
} lambda_t;

typedef struct cell {
	struct cell *car;

	union {
		struct cell *cdr;

		char chr;
		double dbl;
		int64_t i64;

		char *sym;

		struct cell *(*fcn)(struct env *, struct cell *);
	};

	char data[];
} cell_t;

cell_t *cell_cons(cell_t *, cell_t *);
cell_t *cell_cons_t(cell_type_t, ...);
cell_t *cell_dup(cell_t *);

cell_type_t cell_type(cell_t *);
lambda_t *cell_lba(cell_t *);

bool cell_is_atom(cell_t *);
bool cell_is_list(cell_t *);

char *cell_str_intern(char *, unsigned);

#endif

