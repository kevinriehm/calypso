#ifndef CHECK_H
#define CHECK_H

#include <setjmp.h>
#include <stdlib.h>

#include "repl.h"
#include "util.h"

#ifdef NDEBUG
#define check(x, msg) \
	do { \
		if(!(x)) { \
			error("%s",msg); \
			longjmp(checkjmp,1); \
		} \
	} while(0)
#else
#define check(x, msg) \
	do { \
		if(!(x)) { \
			error("%s: %i: %s(): check failed, %s (%s)", \
				__FILE__,__LINE__,__func__,msg,#x); \
			longjmp(checkjmp,1); \
		} \
	} while(0)
#endif

#endif

