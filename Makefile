LYP_CSRC := calypso.c language.tab.c language.yy.c rbtree.c
LYP_CFLAGS := -g

LYP_DEPS := $(LYP_CSRC:.c=.d)
LYP_OBJS := $(LYP_CSRC:.c=.o)

calypso: $(LYP_OBJS)
	$(CC) $(LYP_CFLAGS) -o $@ $^

%.o: %.c
	$(CC) $(LYP_CFLAGS) -c -MMD -o $@ $<

%.yy.c: %.l
	$(LEX) -t $< > $@

%.tab.c: %.y
	$(YACC) -b $* -d $<

-include $(LYP_DEPS)

.PHONY: clean

clean:
	$(RM) calypso
	$(RM) $(LYP_DEPS)
	$(RM) $(LYP_OBJS)

