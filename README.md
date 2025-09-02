# Custom Memory Allocator Project

## Overview
This project implements a dynamic memory allocator in C. It began as a simple implicit free list allocator based on the example in Chapter 9 of CS:APP (Computer Systems, A Programmer's Perspective) by Randal E. Bryant and David R. O' Hallaron.

I improved the implementation to include **explicit** and **segregated free list** allocators, optimized for higher throughput and efficient block management.

The allocator supports:
- `malloc`, `free`, and `realloc` operations
- Efficient block placement and coalescing
- High performance compared to a baseline implicit list implementation

---

## Design

<details>
<summary><strong>General Concepts</strong></summary>

- All allocators use **boundary tag coalescing** to merge adjacent free blocks during `free()` and `realloc()` operations.
- All block headers store **size** and **allocation status**, with the least significant bit indicating allocation.
- **Minimum block size**:
  - Implicit list: 16 bytes (header + footer + payload)
  - Explicit and segregated list: 32 bytes (header + footer + prev/next free pointers)
- **Alignment**: All blocks are 8-byte aligned

</details>

<details>
<summary><strong>Implicit Free List Allocator</strong></summary>

- **List Structure**: Single implicit list of all blocks
- **Placement Policy**: First-fit — searches from the start of the heap to find the first free block large enough
- **Coalescing**: Boundary tag coalescing is applied immediately on free
- **Splitting**: When allocating a block, if the remaining space after allocation ≥ minimum block size, the block is split
- **Prologue/Epilogue**: Allocated blocks at the start and end simplify coalescing and eliminate edge cases
- **Heap Extension**: Uses `sbrk()` to request memory in multiples of `CHUNKSIZE` (4 KB) or adjusted size, whichever is larger
- **Realloc**: Always allocates a new block and copies data; does not attempt in-place expansion

</details>

<details>
<summary><strong>Explicit Free List Allocator</strong></summary>

- **List Structure**: Doubly-linked explicit free list of free blocks
- **Placement Policy**: First-fit traversal of the free list only, avoiding traversal of allocated blocks
- **Insertion Policy**: LIFO insertion at the head of the free list
- **Coalescing**: Boundary tag coalescing on free, maintaining the free list
- **Splitting**: Splits blocks when remainder ≥ `MINBLOCKSIZE`, inserting the remainder back into the free list
- **Heap Extension**: Similar to implicit list; ensures `MINBLOCKSIZE` and initializes free pointers
- **Free List Updates**: `insert_free` and `delete_free` maintain correct `prev`/`next` links for free blocks
- **Realloc Optimizations**:
  - Splits when shrinking
  - Extends in-place by coalescing with next free block when possible
  - Falls back to `malloc` + copy if necessary

</details>

<details>
<summary><strong>Segregated Free List Allocator</strong></summary>

- **List Structure**: Array of 12 segregated free lists, sorted by block size ranges
- **Placement Policy**: First-fit within each segregated list; searches larger lists if no fit found
- **Insertion Policy**: Address-ordered insertion within each size-class list to reduce fragmentation and simplify coalescing
- **Coalescing**: Boundary tag coalescing on free, updating the correct segregated list
- **Splitting**: Splits when remainder ≥ `MINBLOCKSIZE`; remainder is inserted into the correct segregated list
- **Heap Extension**: Maintains minimum block size and initializes free pointers
- **Free List Updates**: Each list maintains `prev`/`next` pointers; insertion maintains address ordering
- **Realloc Optimizations**:
  - Shrinking blocks splits remainder back into segregated list
  - Extends in-place by coalescing with adjacent free blocks if sufficient space exists
  - Falls back to `malloc` + copy if in-place expansion fails

</details>

<details>
<summary><strong>Key Differences Between Allocators</strong></summary>

| Feature                | Implicit                         | Explicit                              | Segregated                                    |
|------------------------|---------------------------------|--------------------------------------|-----------------------------------------------|
| Free list type          | Implicit (all blocks)           | Explicit (doubly-linked free blocks) | Multiple segregated explicit lists            |
| Fit policy              | First-fit over all blocks       | First-fit over free list only         | First-fit within segregated list; search larger lists if needed |
| Free block insertion    | N/A                             | LIFO                                 | Address-ordered by size class                 |
| Splitting               | Yes if remainder ≥ 16           | Yes if remainder ≥ 32                 | Yes if remainder ≥ 32                          |
| Coalescing              | Immediate on free               | Immediate on free                     | Immediate on free                              |
| Heap extension          | `sbrk` in multiples of `CHUNKSIZE` | `sbrk` in multiples of `CHUNKSIZE` | `sbrk` in multiples of `CHUNKSIZE`          |
| Realloc                 | Always `malloc` + copy          | Shrinking and in-place expansion attempted | Shrinking and in-place expansion attempted |

</details>

---

## Benchmark
The allocators were benchmarked against each other and [glibc malloc](https://github.com/lattera/glibc/blob/master/malloc/malloc.c). Tests include:
- Fixed-size `malloc`/`free` throughput (32-byte allocations)
- `realloc` performance (16-byte → 128-byte allocations)

**Results**:

| Allocator | malloc/free 32B | realloc 16→128B | Notes                                |
| --------- | --------------- | --------------- | ------------------------------------ |
| Implicit  | 7.7586 sec      | 18.2553 sec     | Baseline from CS\:APP                |
| Explicit  | 0.004073 sec    | 0.007354 sec    | Fastest throughput for small allocations |
| Segregated| 0.006127 sec    | 0.009320 sec    | Slightly slower than explicit due to multiple lists overhead |
| glibc     | 0.003927 sec    | 0.007754 sec    | Reference system allocator           |

> **Note:** While the explicit free list achieves higher throughput for small allocations in this benchmark, the segregated free list offers better organization and reduces fragmentation for mixed allocation workloads. Raw throughput alone does not fully reflect allocator design tradeoffs.

System Information (benchmark environment): 
- CPU: AMD Ryzen 9 3900X @ 4.92 GHz, 12 cores
- OS: Fedora Linux 42, 64-bit
- Compiler: `gcc` 15.2.1, flags: `-Wall -Wextra -Wpedantic -O3 -g`

---

## Usage
To build the demo, download or clone this repo, then:
```
make
```

And run the benchmark:
```
./demo.out
```
The program outputs throughput for `malloc`/`free` and `realloc` for each allocator, including:
- Custom allocator (segregated free list)
- Implicit free list baseline
- glibc allocator
