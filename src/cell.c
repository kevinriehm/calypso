#include <assert.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "cell.h"
#include "env.h"
#include "htable.h"
#include "util.h"

cell_t *cell_cons(cell_t *car, cell_t *cdr) {
	cell_t *cell;

	cell = malloc(sizeof *cell);
	cell->car = car;
	cell->cdr = cdr;

	return cell;
}

cell_t *cell_cons_t(cell_type_t type, ...) {
	char *str;
	va_list ap;
	int64_t len;
	cell_t *cell;

	va_start(ap,type);

	if(type == VAL_STR) {
		str = va_arg(ap,char *);
		len = va_arg(ap,int64_t);

		cell = malloc((sizeof *cell) + len);
		cell->i64 = len;
		memcpy(cell->data,str,len);
	} else if(type == VAL_LBA) {
		cell = malloc((sizeof *cell) + sizeof(lambda_t));
		memcpy(cell->data,va_arg(ap,lambda_t *),sizeof(lambda_t));
	} else {
		cell = malloc(sizeof *cell);
		switch(type) {
		case VAL_NIL: cell->cdr = NULL; break;
		case VAL_SYM: cell->sym = va_arg(ap,char *);  break;
		case VAL_I64: cell->i64 = va_arg(ap,int64_t); break;
		case VAL_DBL: cell->dbl = va_arg(ap,double);  break;
		case VAL_CHR: cell->chr = va_arg(ap,int);     break;
		case VAL_FCN: cell->fcn = va_arg(ap,fcn_t);   break;

		default: die("unhandled cell type (%i)",type);
		}
	}

	va_end(ap);

	cell->car = (void *) type;

	return cell;
}

cell_t *cell_dup(cell_t *cell) {
	size_t len;
	cell_t *copy;

	switch(cell_type(cell)) {
	case VAL_STR: len = cell->i64;                         break;
	case VAL_LBA: len = (sizeof *cell) + sizeof(lambda_t); break;

	default: len = sizeof *cell; break;
	}

	copy = malloc(len);
	memcpy(copy,cell,len);

	return copy;
}

cell_type_t cell_type(cell_t *cell) {
	uintptr_t type;

	return (type = (uintptr_t) cell->car) < NUM_VAL_TYPES ? type : VAL_LST;
}

lambda_t *cell_lba(cell_t *cell) {
	assert(cell_type(cell) == VAL_LBA);

	return (lambda_t *) cell->data;
}

bool cell_is_atom(cell_t *cell) {
	return !cell || cell_type(cell) != VAL_LST;
}

bool cell_is_list(cell_t *cell) {
	return !cell || cell_type(cell) == VAL_NIL
		|| cell_type(cell) == VAL_LST;
}

char *cell_str_intern(char *str, unsigned len) {
	static htable_t *interned = NULL;

	if(!interned)
		interned = htable_cons(0);

	return htable_intern(interned,str,len);
}

