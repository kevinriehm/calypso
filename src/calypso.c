#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cell.h"
#include "env.h"
#include "mem.h"
#include "repl.h"
#include "util.h"

void grammar_init();

int main(int argc, char **argv) {
	int i;
	FILE *in;
	env_t *globals;
	uint32_t globalsh;

	globalsh = mem_new_handle(GC_TYPE(env_t));
	globals = mem_set_handle(globalsh,env_cons(NULL));

	builtin_init(globals);

	grammar_init();

	if(argc > 1) {
		for(i = 1; i < argc; i++) {
			if(strcmp(argv[i],"-") == 0) {
				filename = "stdin";
				run_file(globals,stdin);
			} else {
				filename = argv[i];
				if(in = fopen(argv[i],"r"))
					run_file(globals,in);
				else die("cannot open '%s'",argv[i]);
				fclose(in);
			}
		}
	} else {
		filename = "stdin";
		run_file(globals,stdin);
	}

	return 0;
}

