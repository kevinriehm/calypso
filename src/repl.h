#ifndef REPL_H
#define REPL_H

#include <setjmp.h>
#include <stdio.h>

struct cell;
struct env;

extern int lineno;
extern char *filename;
extern jmp_buf checkjmp;

void run_file(struct env *, FILE *);

struct cell *eval(struct env *, struct cell *);

#endif

