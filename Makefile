LYP_CSRC := calypso.c cell.c env.c htable.c mem.c repl.c util.c
LYP_RSRC := token.c.re
LYP_YSRC := grammar.y

LYP_CFLAGS := -g -std=c11 -Igen -Isrc -D_POSIX_C_SOURCE=200809L -Wall \
	-Wpedantic -Wextra -Wno-parentheses $(CFLAGS)
LYP_LIBS   := -lm

LYP_DEPS := $(LYP_CSRC:.c=.d) $(LYP_RSRC:.c.re=.d) $(LYP_YSRC:.y=.d)
LYP_OBJS := $(LYP_CSRC:.c=.o) $(LYP_RSRC:.c.re=.o) $(LYP_YSRC:.y=.o)

CBUILD = $(CC) $(LYP_CFLAGS) -c -o $@ $<
DBUILD = $(CC) $(LYP_CFLAGS) -MM -MG -MT obj/$*.o -MF dep/$*.d $<

.PRECIOUS: %/

bin/calypso: $(addprefix obj/,$(LYP_OBJS)) | bin/
	$(CC) $(LYP_CFLAGS) -o $@ $^ $(LYP_LIBS)

dep/%.d: gen/%.c | dep/
	$(DBUILD)

dep/%.d: src/%.c | dep/
	$(DBUILD)

gen/%.c: src/%.rl | gen/
	ragel -G2 -o $@ $<

gen/%.c gen/%.h: src/%.y | gen/
	@cp $< gen/
	@rm -f gen/$*.c gen/$*.h
	lemon -q gen/$*.y

obj/%.o: gen/%.c | obj/
	$(CBUILD)

obj/%.o: src/%.c | obj/
	$(CBUILD)

%/:
	mkdir -p $*

-include $(addprefix dep/, $(LYP_DEPS))

.PHONY: clean

clean:
	rm -rf bin dep gen obj

