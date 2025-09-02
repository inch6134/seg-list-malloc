/*
 * mm.c -
 * Converted to explicit free list allocator, 64-bit headers,
 * LIFO insertion, and MINBLOCKSIZE = 32 bytes.
 */
#include "implicit.h"
#include <assert.h>
#include <memory.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

/////////////////////////////////////////////////////////////////////////////
// Constants and macros (64-bit)
/////////////////////////////////////////////////////////////////////////////
#define WSIZE 8              /* word size (bytes) */
#define DSIZE (2 * WSIZE)    /* doubleword size (bytes) */
#define CHUNKSIZE (1 << 12)  /* initial heap size (bytes) */
#define OVERHEAD (2 * WSIZE) /* overhead of header and footer (bytes) */
#define ALIGNMENT 8          /* memory alignment factor */

/* minimum block: header(8) + footer(8) + next(8) + prev(8) = 32 */
#define MINBLOCKSIZE 32

static inline size_t ALIGN(size_t size) {
  return (((size) + (ALIGNMENT - 1)) & ~(ALIGNMENT - 1));
}

static inline size_t MAX(size_t x, size_t y) { return x > y ? x : y; }

//
// Pack a size and allocated bit into a word
// We mask of the "alloc" field to insure only
// the lower bit is used
//
static inline uint64_t PACK(uint64_t size, int alloc) {
  return (size | (uint64_t)(alloc & 0x1));
}

//
// Read and write an 8 byte word at address p
//
static inline uint64_t GET(void *p) { return *(uint64_t *)p; }
static inline void PUT(void *p, uint64_t val) { *((uint64_t *)p) = val; }

//
// Read the size and allocated fields from address p
//
static inline uint64_t GET_SIZE(void *p) { return GET(p) & ~0x7ULL; }
static inline int GET_ALLOC(void *p) { return (int)GET(p) & 0x1ULL; }

//
// Given block ptr bp, compute address of its header and footer
//
static inline void *HDRP(void *bp) { return ((char *)bp) - WSIZE; }
static inline void *FTRP(void *bp) {
  return ((char *)(bp) + GET_SIZE(HDRP(bp)) - DSIZE);
}

//
// Given block ptr bp, compute address of next and previous blocks
//
static inline void *NEXT_BLKP(void *bp) {
  return ((char *)(bp) + GET_SIZE(((char *)(bp)-WSIZE)));
}

static inline void *PREV_BLKP(void *bp) {
  return ((char *)(bp)-GET_SIZE(((char *)(bp)-DSIZE)));
}

// macros for traversing the free list

static inline void *NEXT_FREE(void *bp) { return (*(void **)(bp)); }
static inline void *PREV_FREE(char *bp) { return (*(void **)(bp + WSIZE)); }

// setters for free-list pointers
static inline void SET_NEXT_FREE(void *bp, void *ptr) {
  *((void **)(bp)) = ptr;
}
static inline void SET_PREV_FREE(char *bp, void *ptr) {
  *((void **)(bp + WSIZE)) = ptr;
}

/////////////////////////////////////////////////////////////////////////////
//
// Global Variables
//

static char *heap_listp; /* pointer to first block */
static void *free_listp; // pointer to first free block

//
// function prototypes for internal helper routines
//
static void *extend_heap(size_t words);
static void place(void *bp, size_t asize);
static void *find_fit(size_t asize);
static void *coalesce(void *bp);
static void delete_free(void *bp);
static void insert_free(void *bp);
static void printblock(void *bp);
static void checkblock(void *bp);

//
// explicit_init - Initialize the memory manager
//
int explicit_init(void) {
  // create initial empty heap w/ padding, prologue, epilogue
  if ((heap_listp = sbrk(4 * WSIZE)) == (void *)-1)
    return -1;

  // initialize empty free list
  PUT(heap_listp, 0);                            // alignment padding
  PUT(heap_listp + (1 * WSIZE), PACK(DSIZE, 1)); // prologue header
  PUT(heap_listp + (2 * WSIZE), PACK(DSIZE, 1)); // prologue footer
  PUT(heap_listp + (3 * WSIZE), PACK(0, 1));     // epilogue header

  heap_listp += DSIZE; // move pointer to prologue

  free_listp = NULL;

  // extend empty heap with a free block of CHUNKSIZE bytes
  if (extend_heap(CHUNKSIZE / WSIZE) == NULL) {
    return -1;
  }
  return 0;
}

//
// extend_heap - Extend heap with free block and return its block pointer
//
static void *extend_heap(size_t words) {
  char *bp;
  size_t size;

  // allocate even number of words to maintain word size alignment
  size = (words % 2) ? (words + 1) * WSIZE : words * WSIZE;

  if (size < MINBLOCKSIZE)
    size = MINBLOCKSIZE;

  if ((long)(bp = sbrk(size)) == -1) {
    return NULL;
  }

  // initialize free block header/footer, rewrite new epilogue header
  PUT(HDRP(bp), PACK(size, 0));         // free block header
  PUT(FTRP(bp), PACK(size, 0));         // free block footer
  PUT(HDRP(NEXT_BLKP(bp)), PACK(0, 1)); // new epilogue header

  // initialize free list pointers
  SET_NEXT_FREE(bp, NULL);
  SET_PREV_FREE(bp, NULL);

  return coalesce(bp);
}

//
// Practice problem 9.8
//
// find_fit - Find a fit for a block with asize bytes
//
static void *find_fit(uint64_t asize) {
  void *bp = free_listp;
  while (bp != NULL) {
    if (asize <= GET_SIZE(HDRP(bp))) {
      return bp;
    }
    bp = NEXT_FREE(bp);
  }
  return NULL; /* no fit */
}

// insert_free/delete_free - LIFO insert and delete from explicit list
static void insert_free(void *bp) {
  assert(GET_ALLOC(HDRP(bp)) == 0);
  SET_NEXT_FREE(bp, free_listp);
  SET_PREV_FREE(bp, NULL);
  if (free_listp != NULL) {
    SET_PREV_FREE(free_listp, bp);
  }
  free_listp = bp;
}

static void delete_free(void *bp) {
  void *prev = PREV_FREE(bp);
  void *next = NEXT_FREE(bp);

  if (prev != NULL) {
    SET_NEXT_FREE(prev, next);
  } else {
    // bp was head
    free_listp = next;
  }

  if (next != NULL) {
    SET_PREV_FREE(next, prev);
  }

  // clear pointers for mem ref safety
  SET_NEXT_FREE(bp, NULL);
  SET_PREV_FREE(bp, NULL);
}

//
// coalesce - boundary tag coalescing. Return ptr to coalesced block
//
static void *coalesce(void *bp) {
  size_t prev_alloc = GET_ALLOC(FTRP(PREV_BLKP(bp)));
  size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp)));
  size_t size = GET_SIZE(HDRP(bp));

  if (prev_alloc && next_alloc) {
    /* Case 1: both allocated */
    insert_free(bp);
    return bp;
  } else if (prev_alloc && !next_alloc) {
    /* Case 2: next is free */
    delete_free(NEXT_BLKP(bp));
    size += GET_SIZE(HDRP(NEXT_BLKP(bp)));
    PUT(HDRP(bp), PACK(size, 0));
    PUT(FTRP(bp), PACK(size, 0));
    insert_free(bp);
  } else if (!prev_alloc && next_alloc) {
    /* Case 3: prev is free */
    void *prev_bp = PREV_BLKP(bp);
    delete_free(prev_bp);
    size += GET_SIZE(HDRP(prev_bp));
    PUT(FTRP(bp), PACK(size, 0));
    PUT(HDRP(prev_bp), PACK(size, 0));
    bp = prev_bp;
    insert_free(bp);
  } else {
    /* Case 4: both prev and next are free */
    void *prev_bp = PREV_BLKP(bp);
    void *next_bp = NEXT_BLKP(bp);
    delete_free(prev_bp);
    delete_free(next_bp);
    size += GET_SIZE(HDRP(prev_bp)) + GET_SIZE(HDRP(next_bp));
    PUT(HDRP(prev_bp), PACK(size, 0));
    PUT(FTRP(next_bp), PACK(size, 0));
    bp = prev_bp;
    insert_free(bp);
  }
  return bp;
}

//
// explicit_malloc - Allocate a block with at least size bytes of payload
//
void *explicit_malloc(uint32_t size) {
  size_t asize;      // adjusted block size
  size_t extendsize; // amount to extend heap if no fit found
  char *bp;

  if (size == 0) { // ignore invalid request
    return NULL;
  }

  // adjust block size to include overhead + alignment
  if (size <= (DSIZE - WSIZE)) {
    asize = MINBLOCKSIZE; // allocate room for free pointers
  } else {                // add in overhead and room for payload
    asize = ALIGN(size + OVERHEAD);
    if (asize < MINBLOCKSIZE)
      asize = MINBLOCKSIZE;
  }

  // search free list for a fit
  if ((bp = find_fit(asize)) != NULL) {
    place(bp, asize);
    return bp;
  }

  // if no fit, request more memory
  extendsize = MAX(asize, CHUNKSIZE);
  if ((bp = extend_heap(extendsize / WSIZE)) == NULL) {
    return NULL;
  }
  place(bp, asize);
  return bp;
}

//
// place - Place block of asize bytes at start of free block bp
//         and split if remainder >= MINBLOCKSIZE, insert remainder if split
//
static void place(void *bp, size_t asize) {
  size_t csize = GET_SIZE(HDRP(bp));

  delete_free(bp); // remove block from free list

  if ((csize - asize) >= MINBLOCKSIZE) {
    // set header/footer
    PUT(HDRP(bp), PACK(asize, 1));
    PUT(FTRP(bp), PACK(asize, 1));

    // remainder becomes a free block
    void *newp = NEXT_BLKP(bp);
    PUT(HDRP(newp), PACK(csize - asize, 0));
    PUT(FTRP(newp), PACK(csize - asize, 0));
    SET_NEXT_FREE(newp, NULL);
    SET_PREV_FREE(newp, NULL);
    insert_free(newp);
  } else { // no splits
    PUT(HDRP(bp), PACK(csize, 1));
    PUT(FTRP(bp), PACK(csize, 1));
  }
}

//
// explicit_free - Free a block
//
void explicit_free(void *bp) {
  size_t size = GET_SIZE(HDRP(bp)); // get block size from header

  PUT(HDRP(bp), PACK(size, 0)); // set header pointer of freed block to 0
  PUT(FTRP(bp), PACK(size, 0)); // set header pointer of freed block to 0

  // Null reference pointers
  SET_NEXT_FREE(bp, NULL);
  SET_PREV_FREE(bp, NULL);

  coalesce(bp); // merge adjacent free blocks
}

//
// explicit_realloc -- implemented for you
//
void *explicit_realloc(void *ptr, uint32_t size) {
  void *newp;
  uint32_t copySize;

  newp = explicit_malloc(size);
  if (newp == NULL) {
    printf("ERROR: explicit_malloc failed in explicit_realloc\n");
    exit(1);
  }
  copySize = (uint32_t)(GET_SIZE(HDRP(ptr)) - OVERHEAD);
  if ((uint32_t)size < copySize) {
    copySize = size;
  }
  memcpy(newp, ptr, copySize);
  explicit_free(ptr);
  return newp;
}

//
// explicit_checkheap - Check the heap for consistency
//
void explicit_checkheap(int verbose) {
  //
  // This provided implementation assumes you're using the structure
  // of the sample solution in the text. If not, omit this code
  // and provide your own explicit_checkheap
  //
  void *bp = heap_listp;

  if (verbose) {
    printf("Heap (%p):\n", heap_listp);
  }

  if ((GET_SIZE(HDRP(heap_listp)) != DSIZE) || !GET_ALLOC(HDRP(heap_listp))) {
    printf("Bad prologue header\n");
  }
  checkblock(heap_listp);

  for (bp = heap_listp; GET_SIZE(HDRP(bp)) > 0; bp = NEXT_BLKP(bp)) {
    if (verbose) {
      printblock(bp);
    }
    checkblock(bp);
  }

  if (verbose) {
    printblock(bp);
  }

  if ((GET_SIZE(HDRP(bp)) != 0) || !(GET_ALLOC(HDRP(bp)))) {
    printf("Bad epilogue header\n");
  }
}

static void printblock(void *bp) {
  uint64_t hsize, halloc, fsize, falloc;

  hsize = GET_SIZE(HDRP(bp));
  halloc = GET_ALLOC(HDRP(bp));
  fsize = GET_SIZE(FTRP(bp));
  falloc = GET_ALLOC(FTRP(bp));

  if (hsize == 0) {
    printf("%p: EOL\n", bp);
    return;
  }

  printf("%p: header: [%d:%c] footer: [%d:%c]\n", bp, (int)hsize,
         (halloc ? 'a' : 'f'), (int)fsize, (falloc ? 'a' : 'f'));
}

static void checkblock(void *bp) {
  if ((uintptr_t)bp % 8) {
    printf("Error: %p is not doubleword aligned\n", bp);
  }
  if (GET(HDRP(bp)) != GET(FTRP(bp))) {
    printf("Error: header does not match footer\n");
  }
}
