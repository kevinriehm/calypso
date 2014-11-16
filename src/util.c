#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "mem.h"

void message(char *type, char *str, ...) {
	va_list ap;

	fprintf(stderr,"%s: ",type);

	va_start(ap,str);
	vfprintf(stderr,str,ap);
	va_end(ap);

	fputc('\n',stderr);
}

void die(char *str, ...) {
	va_list ap;

	fprintf(stderr,"error: ");

	va_start(ap,str);
	vfprintf(stderr,str,ap);
	va_end(ap);

	fputc('\n',stderr);

	exit(EXIT_FAILURE);
}

void *memdup(void *p, size_t n) {
	void *newp;

	newp = mem_alloc(n);
	memcpy(newp,p,n);

	return newp;
}

