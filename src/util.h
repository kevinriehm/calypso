#ifndef UTIL_H
#define UTIL_H

#define error(...) message("error",__VA_ARGS__)
#define info(...)  message("info", __VA_ARGS__)
#define debug(...) message("debug",__VA_ARGS__)

void message(char *, char *, ...);
void die(char *, ...);

void *memdup(void *, size_t);

#endif

