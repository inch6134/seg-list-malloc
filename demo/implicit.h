#include <stdint.h>

extern int mm_init(void);
extern void *mm_malloc(uint32_t size);
extern void mm_free(void *ptr);
extern void *mm_realloc(void *ptr, uint32_t size);
