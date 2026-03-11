#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

void *malloc(size_t size);
void *calloc(size_t nmemb, size_t size);
void free(void *ptr);

// Total wasm linear memory pages × 64 KiB.
// High-water mark: only increases, never decreases even after frees.
size_t total_memory_used_bytes();

// Current live allocation count (increments on malloc, decrements on free).
size_t malloc_live_alloc_count();

// Sum of raw sizes callers passed to malloc() for all live allocations.
// Does not include header overhead or buddy-block rounding waste.
size_t malloc_live_useful_bytes();

// Sum of buddy block sizes for all live allocations.
// Includes 12-byte headers and power-of-2 rounding waste.
size_t malloc_live_block_bytes();

#ifdef __cplusplus
};
#endif
