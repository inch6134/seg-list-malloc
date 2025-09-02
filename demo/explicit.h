#include <stdint.h>

extern int explicit_init(void);
extern void *explicit_malloc(uint32_t size);
extern void explicit_free(void *ptr);
extern void *explicit_realloc(void *ptr, uint32_t size);
