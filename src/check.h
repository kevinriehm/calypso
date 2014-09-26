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
			error("%s:%i: %s",filename,lineno,msg); \
			longjmp(checkjmp,1); \
		} \
	} while(0)
#else
#define check(x, msg) \
	do { \
		if(!(x)) { \
			error("%s: %i: %s(): check failed, file %s, line %i: " \
				"%s",__FILE__,__LINE__,__func__,filename, \
				lineno,msg); \
			longjmp(checkjmp,1); \
		} \
	} while(0)
#endif

#endif

