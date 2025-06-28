#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

void *malloc(size_t size);
void *calloc(size_t nmemb, size_t size);
void free(void *ptr);

// estimate the total memory used by wasm
// the actual memory usage will be higher because the JS part is about 20 MB and not included
size_t total_memory_used_bytes();

#ifdef __cplusplus
};
#endif
