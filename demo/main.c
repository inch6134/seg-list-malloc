#include "../mm.h"
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#define N 100000

static double now_sec() {
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return ts.tv_sec + ts.tv_nsec * 1e-9;
}

int main() {
  void *ptrs[N];
  double start, end;

  // Benchmark custom allocator
  mm_init();
  start = now_sec();
  for (int i = 0; i < N; i++) {
    ptrs[i] = mm_malloc(32);
  }
  for (int i = 0; i < N; i++) {
    mm_free(ptrs[i]);
  }
  end = now_sec();
  printf("Custom allocator: %f sec\n", end - start);

  // Benchmark glibc malloc
  start = now_sec();
  for (int i = 0; i < N; i++) {
    ptrs[i] = malloc(32);
  }
  for (int i = 0; i < N; i++) {
    free(ptrs[i]);
  }
  end = now_sec();
  printf("glibc malloc: %f sec\n", end - start);

  return 0;
}
