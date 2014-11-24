#ifndef VA_MACRO_H
#define VA_MACRO_H

#define EMPTY()
#define DEFER(...) __VA_ARGS__ EMPTY()

#define EXPAND(...)  EXPAND1(EXPAND1(EXPAND1(EXPAND1(EXPAND1(__VA_ARGS__)))))
#define EXPAND1(...) EXPAND2(EXPAND2(EXPAND2(EXPAND2(EXPAND2(__VA_ARGS__)))))
#define EXPAND2(...) EXPAND3(EXPAND3(EXPAND3(EXPAND3(EXPAND3(__VA_ARGS__)))))
#define EXPAND3(...) __VA_ARGS__

#define LITERAL(...) __VA_ARGS__

#define NARGS(...) NARGS__( \
	NARGS_HAS_COMMA(__VA_ARGS__), \
	NARGS_HAS_COMMA(NARGS_COMMA __VA_ARGS__), \
	NARGS_HAS_COMMA(NARGS_COMMA __VA_ARGS__ ()), \
	NARGS_(__VA_ARGS__,25,24,23,22,21,20,19,18,17,16,15,14,13,12,11,10,9, \
		8,7,6,5,4,3,2,1) \
)
#define NARGS_(_1,_2,_3,_4,_5,_6,_7,_8,_9,_10,_11,_12,_13,_14,_15,_16,_17, \
	_18,_19,_20,_21,_22,_23,_24,_25,n, ...) n
#define NARGS_HAS_COMMA(...) NARGS_(__VA_ARGS__,1,1,1,1,1,1,1,1,1,1,1,1,1,1, \
	1,1,1,1,1,1,1,1,1,1,0)
#define NARGS_COMMA(...) ,
#define NARGS__(a, b, c, n) NARGS___(a,b,c,n)
#define NARGS___(a, b, c, n) NARGS___##a##b##c(n)
#define NARGS___001(n) 0
#define NARGS___000(n) 1
#define NARGS___011(n) 1
#define NARGS___111(n) n

#define VAR_ARG(base, ...) VAR_ARG_(base,NARGS(__VA_ARGS__),__VA_ARGS__)
#define VAR_ARG_(base, num, ...) VAR_ARG__(base,num,__VA_ARGS__)
#define VAR_ARG__(base, num, ...) base##num(__VA_ARGS__)

#define EACH_LAST(f, all, x, nx) EACH_LAST_(f,all,x,nx)
#define EACH_LAST_(f, all, x, nx) EACH_LAST__##nx(f,all,x)
#define EACH_LAST__0(f, all, x)
#define EACH_LAST__1(f, all, x) f(LITERAL all, x)

#define EACH3(f, sep, all)
#define EACH4(f, sep, all, x) EACH_LAST(f,all,x,NARGS(x))
#define EACH5(f, sep, all, x, ...) \
	f(LITERAL all,x) LITERAL sep EACH4(f,sep,all,__VA_ARGS__)
#define EACH6(f, sep, all, x, ...) \
	f(LITERAL all,x) LITERAL sep EACH5(f,sep,all,__VA_ARGS__)
#define EACH7(f, sep, all, x, ...) \
	f(LITERAL all,x) LITERAL sep EACH6(f,sep,all,__VA_ARGS__)
#define EACH8(f, sep, all, x, ...) \
	f(LITERAL all,x) LITERAL sep EACH7(f,sep,all,__VA_ARGS__)
#define EACH9(f, sep, all, x, ...) \
	f(LITERAL all,x) LITERAL sep EACH8(f,sep,all,__VA_ARGS__)
#define EACH10(f, sep, all, x, ...) \
	f(LITERAL all,x) LITERAL sep EACH9(f,sep,all,__VA_ARGS__)
#define EACH11(f, sep, all, x, ...) \
	f(LITERAL all,x) LITERAL sep EACH10(f,sep,all,__VA_ARGS__)
#define EACH12(f, sep, all, x, ...) \
	f(LITERAL all,x) LITERAL sep EACH11(f,sep,all,__VA_ARGS__)
#define EACH13(f, sep, all, x, ...) \
	f(LITERAL all,x) LITERAL sep EACH12(f,sep,all,__VA_ARGS__)
#define EACH14(f, sep, all, x, ...) \
	f(LITERAL all,x) LITERAL sep EACH13(f,sep,all,__VA_ARGS__)
#define EACH15(f, sep, all, x, ...) \
	f(LITERAL all,x) LITERAL sep EACH14(f,sep,all,__VA_ARGS__)
#define EACH16(f, sep, all, x, ...) \
	f(LITERAL all,x) LITERAL sep EACH15(f,sep,all,__VA_ARGS__)
#define EACH17(f, sep, all, x, ...) \
	f(LITERAL all,x) LITERAL sep EACH16(f,sep,all,__VA_ARGS__)
#define EACH18(f, sep, all, x, ...) \
	f(LITERAL all,x) LITERAL sep EACH17(f,sep,all,__VA_ARGS__)
#define EACH19(f, sep, all, x, ...) \
	f(LITERAL all,x) LITERAL sep EACH18(f,sep,all,__VA_ARGS__)
#define EACH20(f, sep, all, x, ...) \
	f(LITERAL all,x) LITERAL sep EACH19(f,sep,all,__VA_ARGS__)
#define EACH21(f, sep, all, x, ...) \
	f(LITERAL all,x) LITERAL sep EACH20(f,sep,all,__VA_ARGS__)
#define EACH22(f, sep, all, x, ...) \
	f(LITERAL all,x) LITERAL sep EACH21(f,sep,all,__VA_ARGS__)
#define EACH23(f, sep, all, x, ...) \
	f(LITERAL all,x) LITERAL sep EACH22(f,sep,all,__VA_ARGS__)
#define EACH24(f, sep, all, x, ...) \
	f(LITERAL all,x) LITERAL sep EACH23(f,sep,all,__VA_ARGS__)
#define EACH25(f, sep, all, x, ...) \
	f(LITERAL all,x) LITERAL sep EACH24(f,sep,all,__VA_ARGS__)

#define EACH(...) VAR_ARG(EACH,__VA_ARGS__)
#define EACH_INDIRECT() EACH

#endif

