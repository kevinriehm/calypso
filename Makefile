LYP_CSRC := calypso.c builtin.c cell.c env.c htable.c rbtree.c repl.c util.c
LYP_LSRC := language.l
LYP_YSRC := language.y

LYP_CFLAGS := -g --std=c11 -Igen -Isrc -D_POSIX_C_SOURCE=200809L $(CFLAGS)
LYP_LIBS   := -lm

LYP_DEPS := $(LYP_CSRC:.c=.d) $(LYP_LSRC:.l=.yy.d) $(LYP_YSRC:.y=.tab.d)
LYP_OBJS := $(LYP_CSRC:.c=.o) $(LYP_LSRC:.l=.yy.o) $(LYP_YSRC:.y=.tab.o)

CBUILD = $(CC) -c -MMD -MF dep/$*.d -MT obj/$*.o $(LYP_CFLAGS) -o $@ $<

.PRECIOUS: %/

bin/calypso: $(addprefix obj/,$(LYP_OBJS)) | bin/
	$(CC) $(LYP_CFLAGS) -o $@ $^ $(LYP_LIBS)

gen/%.yy.c: src/%.l gen/%.tab.h | gen/
	$(LEX) -t $< > $@

gen/%.tab.c gen/%.tab.h: src/%.y | gen/
	$(YACC) -b gen/$* -d $<

obj/%.o: gen/%.c | dep/ obj/
	$(CBUILD)

obj/%.o: src/%.c | dep/ obj/
	$(CBUILD)

%/:
	mkdir -p $*

-include $(addprefix dep/, $(LYP_DEPS))

.PHONY: clean

clean:
	$(RM) -r bin dep gen obj

