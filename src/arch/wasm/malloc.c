#include <stddef.h>
#include <string.h>

// cell bisecting implementation of malloc
// as described in
// https://github.com/miguelperes/custom-malloc
//
// Block header layout — three size_t words immediately before the pointer
// returned to the caller:
//
//   BLKHDR_SIZE    [ptr-3]  buddy block size (power of 2); valid in both states
//   BLKHDR_NEXT    [ptr-2]  next-free pointer when FREE; 0 when ALLOCATED
//   BLKHDR_PAYLOAD [ptr-1]  prev-free pointer when FREE;
//                           caller's original request size when ALLOCATED
//
// Slot [ptr-1] (BLKHDR_PAYLOAD) is safely reused because:
//   - when a block is on the free list, it holds the prev pointer
//   - when a block is allocated, _remove_block sets BLKHDR_NEXT to 0 and
//     malloc immediately writes the request size into BLKHDR_PAYLOAD before
//     returning — so free() can always recover the original size from there

#define BLKHDR_SIZE(b)    (((size_t*)(b))[0])
#define BLKHDR_NEXT(b)    (((size_t*)(b))[1])
#define BLKHDR_PAYLOAD(b) (((size_t*)(b))[2])

#ifndef MALLOC_MIN_BLOCK
#define MALLOC_MIN_BLOCK 4
#endif
#ifndef MALLOC_INITIAL_ROOT
#define MALLOC_INITIAL_ROOT MALLOC_MIN_BLOCK
#endif

#define SIZE_SIZE_T 4
const size_t __min_block_size = 1 << MALLOC_MIN_BLOCK;

extern unsigned char __heap_base;
size_t __first_free = (size_t)&__heap_base;
size_t __root_size = 1 << MALLOC_INITIAL_ROOT;
int __memory_status = 0;

static size_t _live_alloc_count  = 0;
static size_t _live_useful_bytes = 0;
static size_t _live_block_bytes  = 0;

size_t memory_size(void) { return __builtin_wasm_memory_size(0); }

size_t memory_grow(int delta) { return __builtin_wasm_memory_grow(0, delta); }

void _ensure_root_size(size_t total) {
  total += (size_t)&__heap_base;
  size_t size = memory_size() << 16;
  if (total > size) {
    // no checking return code. assume it succeeds :)
    memory_grow((total >> 16) - (size >> 16) + 1);
  }
}

void _link_block(size_t before, size_t after) {
  BLKHDR_NEXT(before)    = after;
  BLKHDR_PAYLOAD(after)  = before;
}

void _remove_block(size_t block) {
  // fix before and after
  size_t before = BLKHDR_PAYLOAD(block);
  size_t after  = BLKHDR_NEXT(block);
  _link_block(before, after);
  // fix first
  if (__first_free == block) {
    // set to something else
    __first_free = BLKHDR_NEXT(block);
    // if it was the only block
    if (__first_free == block) {
      // indicate with a status
      __memory_status = 2;
    }
  }
  // mark used (BLKHDR_NEXT == 0 is the "allocated" sentinel)
  BLKHDR_NEXT(block) = 0;
}

void _append_tail(size_t r) {
  if (__memory_status == 2) {
    // all blocks were used, now we have a block
    __first_free = r;
    _link_block(__first_free, __first_free);
    return;
  }
  // add block back to list
  size_t second_free = BLKHDR_NEXT(__first_free);
  _link_block(__first_free, r);
  _link_block(r, second_free);
}

void *_malloc_search_block(size_t n) {
  // search for suitable block
  size_t cur_block = __first_free;
  do {
    size_t cur_block_size = BLKHDR_SIZE(cur_block);
    size_t next_block     = BLKHDR_NEXT(cur_block);
    if (cur_block_size < n) {
      cur_block = next_block;
      continue;
    }
    // this block is large enough
    // split the block if it's too large
    // but still above the minimum size
    while (cur_block_size > __min_block_size && 2 * n <= cur_block_size) {
      // mitosis
      cur_block_size >>= 1;
      size_t sibling_block = cur_block + cur_block_size;
      BLKHDR_SIZE(sibling_block) = BLKHDR_SIZE(cur_block) = cur_block_size;
      _append_tail(sibling_block);
    }
    _remove_block(cur_block);
    return (void *)(cur_block + SIZE_SIZE_T * 3);
  } while (cur_block != __first_free);
  // failed to find a block
  return NULL;
}

static void _track_alloc(void *result, size_t useful_n) {
  size_t block = (size_t)result - SIZE_SIZE_T * 3;
  BLKHDR_PAYLOAD(block) = useful_n;
  _live_useful_bytes += useful_n;
  _live_block_bytes  += BLKHDR_SIZE(block);
  _live_alloc_count++;
}

void *malloc(size_t n) {
  // word size assumptions
  _Static_assert(sizeof(size_t) == SIZE_SIZE_T,
                 "hardcoded value for sizeof(size_t) is incorrect");
  // 0 case
  if (n == 0)
    return NULL;
  // save caller's request before any transforms
  size_t useful_n = n;
  // alignment
  n = -((-n) & ~(SIZE_SIZE_T - 1));
  // account for header size
  n += SIZE_SIZE_T * 3;
  // first time: init available memory
  if (__memory_status == 0) {
    __memory_status = 1;
    _ensure_root_size(__root_size);
    BLKHDR_SIZE(__first_free) = __root_size;
    _link_block(__first_free, __first_free);
  }
  // all memory used: expand
  if (__memory_status == 2) {
    __memory_status = 1;
    _ensure_root_size(__root_size << 1);
    __first_free = ((size_t)&__heap_base) + __root_size;
    BLKHDR_SIZE(__first_free) = __root_size;
    _link_block(__first_free, __first_free);
    __root_size <<= 1;
  }
  // search for suitable block
  void *result = _malloc_search_block(n);
  if (result != NULL) {
    _track_alloc(result, useful_n);
    return result;
  }
  // memory available, but no blocks large enough: expand
  do {
    _ensure_root_size(__root_size << 1);
    size_t big_block = ((size_t)&__heap_base) + __root_size;
    BLKHDR_SIZE(big_block) = __root_size;
    _append_tail(big_block);
    __root_size <<= 1;
  } while ((__root_size >> 1) < n);
  // there will definitely be a block available this time
  result = _malloc_search_block(n);
  if (result != NULL)
    _track_alloc(result, useful_n);
  return result;
}

void *calloc(size_t nmemb, size_t size) {
  void *mem = malloc(nmemb * size);
  memset(mem, 0, nmemb * size);

  return mem;
}

void free(void *p) {
  const size_t root = (size_t)&__heap_base;
  // null case
  if (p == NULL)
    return;
  size_t r = (size_t)p - SIZE_SIZE_T * 3;
  // update live trackers before the merge loop changes BLKHDR_SIZE(r)
  _live_useful_bytes -= BLKHDR_PAYLOAD(r);
  _live_block_bytes  -= BLKHDR_SIZE(r);
  _live_alloc_count--;
  // try to merge siblings
  size_t r_size = BLKHDR_SIZE(r);
  while (1) {
    if (r_size >= __root_size) {
      // already the largest
      break;
    }
    size_t sibling_block;
    size_t is_on_right = (r - root + r_size) & r_size;
    if (is_on_right) {
      // sibling on right
      sibling_block = r + r_size;
    } else {
      // sibling on left
      sibling_block = r - r_size;
    }
    // BLKHDR_NEXT == 0 means the sibling is allocated; can't merge
    if (BLKHDR_NEXT(sibling_block) == 0) {
      break;
    }
    size_t sibling_block_size = BLKHDR_SIZE(sibling_block);
    if (sibling_block_size != r_size) {
      // sibling has wrong size (possibly split), can't merge further
      break;
    }
    _remove_block(sibling_block);
    if (!is_on_right) {
      r = sibling_block;
    }
    BLKHDR_SIZE(r) = r_size = r_size << 1;
  }
  // add block back to free list
  _append_tail(r);
}

size_t total_memory_used_bytes() {
  // each page is 64 KiB — high-water mark, never decreases
  return memory_size() << 16;
}

size_t malloc_live_alloc_count()  { return _live_alloc_count;  }
size_t malloc_live_useful_bytes() { return _live_useful_bytes; }
size_t malloc_live_block_bytes()  { return _live_block_bytes;  }
