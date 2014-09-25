#ifndef REPL_H
#define REPL_H

#include <stdio.h>

struct cell;
struct env;

void run_file(struct env *, FILE *);

struct cell *eval(struct env *, struct cell *);

#endif

