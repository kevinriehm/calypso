LYP_CSRC := calypso.c rbtree.c
LYP_LSRC := language.l
LYP_YSRC := language.y

LYP_CFLAGS := -g -Igen -Isrc

LYP_DEPS := $(LYP_CSRC:.c=.d) $(LYP_LSRC:.l=.yy.d) $(LYP_YSRC:.y=.tab.d)
LYP_OBJS := $(LYP_CSRC:.c=.o) $(LYP_LSRC:.l=.yy.o) $(LYP_YSRC:.y=.tab.o)

CBUILD = $(CC) -c -MMD -MF dep/$*.d -MT obj/$*.o $(LYP_CFLAGS) -o $@ $<

bin/calypso: $(addprefix obj/,$(LYP_OBJS))
	$(CC) $(LYP_CFLAGS) -o $@ $^

gen/%.yy.c: src/%.l gen/%.tab.h
	$(LEX) -t $< > $@

gen/%.tab.c gen/%.tab.h: src/%.y
	$(YACC) -b gen/$* -d $<

obj/%.o: gen/%.c
	$(CBUILD)

obj/%.o: src/%.c
	$(CBUILD)

-include $(addprefix dep/, $(LYP_DEPS))

.PHONY: clean

clean:
	$(RM) bin/*
	$(RM) dep/*
	$(RM) gen/*
	$(RM) obj/*

