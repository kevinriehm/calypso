#ifndef UTIL_H
#define UTIL_H

#ifndef MESSAGE_LEVEL
#define MESSAGE_LEVEL 0
#endif

#if MESSAGE_LEVEL >= 0
#define error(...) message("error",__VA_ARGS__)
#else
#define error(...)
#endif

#if MESSAGE_LEVEL >= 1
#define info(...)  message("info", __VA_ARGS__)
#else
#define info(...)
#endif

#if MESSAGE_LEVEL >= 2
#define debug(...) message("debug",__VA_ARGS__)
#else
#define debug(...)
#endif

void message(char *, char *, ...);
void die(char *, ...);

void *memdup(void *, size_t);

#endif

