#include "../mm.h"
#include "explicit.h"
#include "implicit.h"
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include <time.h>
#include <unistd.h>

#define N 100000
#define MAX_SIZE 1024
#define UTIL_N 1000
#define UTIL_OPS 50000

// wrappers for glibc function calls
static void *glibc_malloc(uint32_t size) { return malloc((size_t)size); }

static void *glibc_realloc(void *ptr, uint32_t size) {
  return realloc(ptr, (size_t)size);
}

// timing utility
static double now_sec() {
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return ts.tv_sec + ts.tv_nsec * 1e-9;
}

static void benchmark_malloc_free(const char *name,
                                  void *(*my_malloc)(uint32_t),
                                  void (*my_free)(void *)) {
  void *ptrs[N];
  double start, end;

  start = now_sec();

  for (int i = 0; i < N; i++) {
    ptrs[i] = my_malloc(32);
  }

  for (int i = 0; i < N; i++) {
    my_free(ptrs[i]);
  }

  end = now_sec();
  printf("%s malloc/free throughput (fixed 32B): %.6f sec\n", name,
         end - start);
}

static void benchmark_realloc(const char *name, void *(*my_malloc)(uint32_t),
                              void (*my_free)(void *),
                              void *(*my_realloc)(void *, uint32_t)) {
  void *ptrs[N];
  double start, end;

  for (int i = 0; i < N; i++) {
    ptrs[i] = my_malloc(16);
  }

  start = now_sec();

  for (int i = 0; i < N; i++) {
    ptrs[i] = my_realloc(ptrs[i], 128);
  }

  end = now_sec();

  for (int i = 0; i < N; i++) {
    my_free(ptrs[i]);
  }

  printf("%s realloc throughput (16 -> 128B): %.6f sec\n", name, end - start);
}

int main() {
  printf("=== Memory Allocator Benchmark Demo ===\n\n");

  printf("Number of allocations per test: %d\n", N);
  printf("Max allocation size in utilization test: %d bytes\n", MAX_SIZE);
  printf("Number of pointers tracked for utilization: %d\n\n", UTIL_N);

  // Custom allocator
  printf(">>> Testing Custom allocator (segregated free list) <<<\n");
  mm_init();
  benchmark_malloc_free("Custom", mm_malloc, mm_free);
  benchmark_realloc("Custom", mm_malloc, mm_free, mm_realloc);
  putchar('\n');

  // Implicit list baseline
  printf(">>> Testing Implicit list allocator <<<\n");
  implicit_init();
  benchmark_malloc_free("Implicit", implicit_malloc, implicit_free);
  benchmark_realloc("Implicit", implicit_malloc, implicit_free,
                    implicit_realloc);
  putchar('\n');

  // Explicit list baseline
  printf(">>> Testing Explicit list allocator <<<\n");
  explicit_init();
  benchmark_malloc_free("Explicit", explicit_malloc, explicit_free);
  benchmark_realloc("Explicit", explicit_malloc, explicit_free,
                    explicit_realloc);
  putchar('\n');

  // glibc allocator
  printf(">>> Testing glibc malloc <<<\n");
  benchmark_malloc_free("glibc", glibc_malloc, free);
  benchmark_realloc("glibc", glibc_malloc, free, glibc_realloc);
  putchar('\n');

  printf("=== Benchmark Complete ===\n");
  return 0;
}
