#include <stdio.h>
#include <stdlib.h>
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

