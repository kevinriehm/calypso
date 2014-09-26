#ifndef CHECK_H
#define CHECK_H

#include <setjmp.h>
#include <stdlib.h>

#include "repl.h"
#include "util.h"

#ifdef NDEBUG
#define check(x) \
	do { \
		if(!(x)) { \
			error("runtime exception"); \
			longjmp(checkjmp,1); \
		} \
	} while(0)
#else
#define check(x) \
	do { \
		if(!(x)) { \
			error("%s: %i: %s(): check failed ("#x")",__FILE__, \
				__LINE__,__func__); \
			longjmp(checkjmp,1); \
		} \
	} while(0)
#endif

#endif

