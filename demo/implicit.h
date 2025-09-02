#include <stdint.h>

extern int implicit_init(void);
extern void *implicit_malloc(uint32_t size);
extern void implicit_free(void *ptr);
extern void *implicit_realloc(void *ptr, uint32_t size);
