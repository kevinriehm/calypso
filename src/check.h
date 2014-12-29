#ifndef CHECK_H
#define CHECK_H

#include <inttypes.h>
#include <setjmp.h>
#include <stdlib.h>

#include "repl.h"
#include "token.h"
#include "util.h"

#ifdef NDEBUG
#define check(x, msg) \
	do { \
		if(!(x)) { \
			error("%s:%" PRIu32 ": %s",filename, \
				stream_lineno(currentstream),msg); \
			longjmp(checkjmp,1); \
		} \
	} while(0)
#else
#define check(x, msg) \
	do { \
		if(!(x)) { \
			error("%s: %i: %s(): check failed, file %s, line %" \
				PRIu32 ": %s",__FILE__,__LINE__,__func__, \
				filename,stream_lineno(currentstream),msg); \
			longjmp(checkjmp,1); \
		} \
	} while(0)
#endif

#endif

