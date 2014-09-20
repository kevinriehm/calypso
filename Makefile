LYP_CSRC := calypso.c rbtree.c
LYP_LSRC := language.l
LYP_YSRC := language.y

LYP_CFLAGS := -g

LYP_DEPS := $(LYP_CSRC:.c=.d) $(LYP_LSRC:.l=.yy.d) $(LYP_YSRC:.y=.tab.d)
LYP_OBJS := $(LYP_CSRC:.c=.o) $(LYP_LSRC:.l=.yy.o) $(LYP_YSRC:.y=.tab.o)

bin/calypso: $(addprefix obj/,$(LYP_OBJS))
	$(CC) $(LYP_CFLAGS) -o $@ $^

%.yy.c: %.l %.tab.h
	$(LEX) -t $< > $@

%.tab.c %.tab.h: %.y
	$(YACC) -b $* -d $<

obj/%.o: src/%.c
	$(CC) -c -MMD -MF dep/$*.d -MT obj/$*.o $(LYP_CFLAGS) -o $@ $<

-include $(addprefix dep/, $(LYP_DEPS))

.PHONY: clean

clean:
	$(RM) bin/*
	$(RM) dep/*
	$(RM) obj/*

