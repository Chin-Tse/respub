
#ifndef BENCH_H
#define BENCH_H 1

#define __need_timespec
#define __USE_POSIX199309

#include <time.h>
#include "/usr/include/time.h"

/**
 * Get the current wall time.
 */

float
wall(void);

/**
 * Get the current cpu time.
 */

float
cpu(void);


#ifdef CLOCK_MONOTONIC_RAW
#  define CLOCK_SUITABLE CLOCK_MONOTONIC_RAW
#else
#  define CLOCK_SUITABLE CLOCK_MONOTONIC
#endif

float
wall(void) {
  struct timespec tp;
  clock_gettime(CLOCK_SUITABLE, &tp);
  return tp.tv_sec + 1e-9 * tp.tv_nsec;
}

float
cpu(void) {
  struct timespec tp;
  clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &tp);
  return tp.tv_sec + 1e-9 * tp.tv_nsec;
}


#endif

