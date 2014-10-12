#ifndef REPL_H
#define REPL_H

#include <setjmp.h>
#include <stdio.h>

struct cell;
struct env;
struct lambda;

extern int lineno;
extern char *filename;
extern jmp_buf checkjmp;

void run_file(struct env *, FILE *);

struct cell *expand_macro(struct lambda *, struct cell *);
struct cell *eval(struct env *, struct cell *);

#endif

