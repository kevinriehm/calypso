#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

#include "cell.h"
#include "env.h"
#include "util.h"

cell_t *cell_cons(cell_t *car, cell_t *cdr) {
	cell_t *cell;

	cell = malloc(sizeof *cell);
	cell->car.p = car;
	cell->cdr.p = cdr;

	return cell;
}

cell_t *cell_cons_t(enum cell_type type, ...) {
	va_list ap;
	cell_t *cell;

	cell = malloc(sizeof *cell);
	cell->car.type = type;

	va_start(ap,type);

	switch(type) {
	case VAL_NIL: cell->cdr.p = NULL; break;
	case VAL_SYM: cell->cdr.str = va_arg(ap,char *);     break;
	case VAL_I64: cell->cdr.i64 = va_arg(ap,int64_t);    break;
	case VAL_DBL: cell->cdr.dbl = va_arg(ap,double);     break;
	case VAL_CHR: cell->cdr.chr = va_arg(ap,int);        break;
	case VAL_STR: cell->cdr.str = va_arg(ap,char *);     break;
	case VAL_LBA: cell->cdr.lba = va_arg(ap,lambda_t *); break;

	case VAL_FCN:
		cell->cdr.fcn = va_arg(ap,
			cell_t *(*)(env_t *, cell_t *));
		break;

	default: die("unhandled cell type (%i)",type);
	}

	va_end(ap);

	return cell;
}

cell_t *cell_dup(cell_t *cell) {
	cell_t *copy;

	copy = malloc(sizeof *copy);
	memcpy(copy,cell,sizeof *copy);

	return copy;
}

