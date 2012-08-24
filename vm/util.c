#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stdint.h>
#include <sys/time.h>
#include "util.h"

uint64_t now_usec() {
  struct timeval tv;
  if(gettimeofday(&tv,0)) {
    die(131,"gettimeofday failed");
  }
  return tv.tv_sec * 1000000L + tv.tv_usec;
}

void die(int code,char *msg) {
  fprintf(stderr,"%s\n",msg);
  exit(code);
}

char * make_message(const char *fmt, ...) {
  /* Guess we need no more than 100 bytes. */
  int n, size = 100;
  char *p, *np;
  va_list ap;

  if((p = malloc(size)) == NULL)
   return NULL;

  while (1) {
    /* Try to print in the allocated space. */
    va_start(ap, fmt);
    n = vsnprintf(p, size, fmt, ap);
    va_end(ap);
    /* If that worked, return the string. */
    if (n > -1 && n < size)
      return p;
    /* Else try again with more space. */
    if (n > -1)    /* glibc 2.1 */
      size = n+1; /* precisely what is needed */
    else           /* glibc 2.0 */
      size *= 2;  /* twice the old size */
    if ((np = realloc (p, size)) == NULL) {
      free(p);
      return NULL;
    } else {
      p = np;
    }
  }
}

void byte_count(uint64_t val,int *num,char *unit) {
  char *units = "bkMGT",*u;
  for(u=units;*(u+1);u++) {
    if(val<2048) {
      break;
    }
    val /= 1024;
  }
  *num = val;
  *unit = *u;
}
