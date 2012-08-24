#ifndef UTIL_H
#define UTIL_H

#include <stdint.h>

uint64_t now_usec();
void die(int,char *);
char * make_message(const char *, ...);
void byte_count(uint64_t,int *,char *);

#endif

