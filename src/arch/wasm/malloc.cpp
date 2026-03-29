#ifdef WASM_MEMORY_BACKEND
#include "buddy_allocator.hpp"

extern unsigned char __heap_base;

static BuddyAllocator g_alloc(&__heap_base, 0);

extern "C" {

void *malloc(size_t n)                  { return g_alloc.malloc(n); }
void *calloc(size_t nmemb, size_t size) { return g_alloc.calloc(nmemb, size); }
void  free(void *p)                     { g_alloc.free(p); }
size_t total_memory_used_bytes()        { return g_alloc.total_memory_used_bytes(); }
size_t malloc_live_alloc_count()        { return g_alloc.live_alloc_count_get(); }
size_t malloc_live_useful_bytes()       { return g_alloc.live_useful_bytes_get(); }
size_t malloc_live_block_bytes()        { return g_alloc.live_block_bytes_get(); }

} // extern "C"
#endif
