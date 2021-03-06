#include <assert.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "cell.h"
#include "env.h"
#include "htable.h"
#include "mem.h"
#include "util.h"

cell_t *cell_cons(cell_t *car, cell_t *cdr) {
	cell_t *cell;

	cell = mem_alloc(sizeof *cell);
	cell->car = car;
	cell->cdr = cdr;

	return cell;
}

cell_t *cell_cons_t(cell_type_t type, ...) {
	va_list ap;
	cell_t *cell;

	va_start(ap,type);

	if(type == VAL_LBA) {
		cell = mem_alloc((sizeof *cell) + sizeof(lambda_t));
		memcpy(cell->data,va_arg(ap,lambda_t *),sizeof(lambda_t));
	} else {
		cell = mem_alloc(sizeof *cell);
		switch(type) {
		case VAL_NIL: cell->cdr = NULL; break;
		case VAL_SYM: cell->sym = va_arg(ap,string_t *); break;
		case VAL_I64: cell->i64 = va_arg(ap,int64_t);    break;
		case VAL_DBL: cell->dbl = va_arg(ap,double);     break;
		case VAL_CHR: cell->chr = va_arg(ap,int);        break;
		case VAL_STR: cell->str = va_arg(ap,string_t *); break;
		case VAL_FCN: cell->fcn = va_arg(ap,fcn_t);      break;

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

	copy = mem_alloc(len);
	memcpy(copy,cell,len);

	return copy;
}

cell_type_t cell_type(cell_t *cell) {
	uintptr_t type;

	type = cell ? (uintptr_t) cell->car : VAL_LST;

	return type != VAL_NIL && type < NUM_VAL_TYPES ? type : VAL_LST;
}

lambda_t *cell_lba(cell_t *cell) {
	assert(cell_type(cell) == VAL_LBA);

	return (lambda_t *) cell->data;
}

bool cell_is_atom(cell_t *cell) {
	return !cell || cell_type(cell) != VAL_NIL
		&& cell_type(cell) != VAL_LST;
}

bool cell_is_list(cell_t *cell) {
	return !cell || cell_type(cell) == VAL_NIL
		|| cell_type(cell) == VAL_LST;
}

string_t *cell_str_cons(char *cstr, size_t len) {
	string_t *str;

	str = mem_alloc(sizeof *str + len);
	memcpy(str->str,cstr,str->len = len);

	return str;
}

string_t *cell_str_intern(string_t *str) {
	static uint32_t internedh;
	static htable_t *interned = NULL;

	if(!interned) {
		internedh = mem_new_handle(GC_TYPE(htable_t));
		interned = mem_set_handle(internedh,htable_cons(0));
	}

	return htable_intern(interned,str,sizeof *str + str->len);
}

